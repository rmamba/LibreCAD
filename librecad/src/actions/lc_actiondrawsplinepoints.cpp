/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2014 Dongxu Li (dongxuli2011@gmail.com)
** Copyright (C) 2014 Pavel Krejcir (pavel@pamsoft.cz)
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!
**
**********************************************************************/

#include <QAction>
#include <QMouseEvent>
#include "lc_actiondrawsplinepoints.h"

#include "lc_splinepoints.h"
#include "rs_dialogfactory.h"
#include "rs_graphicview.h"
#include "rs_commands.h"
#include "rs_commandevent.h"
#include "rs_point.h"
#include "rs_coordinateevent.h"

LC_ActionDrawSplinePoints::LC_ActionDrawSplinePoints(RS_EntityContainer& container,
													 RS_GraphicView& graphicView):
	RS_ActionDrawSpline( container, graphicView)
  ,data(new LC_SplinePointsData(false, false))
  ,spline(nullptr)
{
	actionType=RS2::ActionDrawSplinePoints;
	setName("DrawSplinePoints");
}

LC_ActionDrawSplinePoints::~LC_ActionDrawSplinePoints() {}

void LC_ActionDrawSplinePoints::reset() {
	spline.reset();
	undoBuffer.clear();
}

void LC_ActionDrawSplinePoints::init(int status)
{
	RS_PreviewActionInterface::init(status);
	reset();
}

void LC_ActionDrawSplinePoints::trigger()
{
	if(!spline.get()) return;

	spline->setLayerToActive();
	spline->setPenToActive();
	spline->update();
	RS_Entity* s=spline->clone();
	container->addEntity(s);

	// upd. undo list:
	if(document != NULL)
	{
		document->startUndoCycle();
		document->addUndoable(s);
		document->endUndoCycle();
	}

	// upd view
	RS_Vector r = graphicView->getRelativeZero();
	graphicView->redraw(RS2::RedrawDrawing);
	graphicView->moveRelativeZero(r);
	RS_DEBUG->print("RS_ActionDrawSplinePoints::trigger(): spline added: %d",
		s->getId());

	reset();
}

void LC_ActionDrawSplinePoints::mouseMoveEvent(QMouseEvent* e)
{
	RS_DEBUG->print("RS_ActionDrawSplinePoints::mouseMoveEvent begin");

	RS_Vector mouse = snapPoint(e);

	if(getStatus() == SetNextPoint)
	{
		LC_SplinePoints*  sp = static_cast<LC_SplinePoints*>(spline->clone());
		sp->addPoint(mouse);
		deletePreview();
		preview->addEntity(sp);

		for(auto const& v: sp->getPoints())
		{
			preview->addEntity(new RS_Point(preview.get(), RS_PointData(v)));
		}
		drawPreview();
	}

	RS_DEBUG->print("RS_ActionDrawSplinePoints::mouseMoveEvent end");
}

void LC_ActionDrawSplinePoints::mouseReleaseEvent(QMouseEvent* e)
{
	if(e->button() == Qt::LeftButton)
	{
		RS_CoordinateEvent ce(snapPoint(e));
		coordinateEvent(&ce);
	}
	else if(e->button() == Qt::RightButton)
	{
		if(getStatus() == SetNextPoint && spline.get())
		{
			trigger();
		}
		init(getStatus() - 1);
	}
}

void LC_ActionDrawSplinePoints::coordinateEvent(RS_CoordinateEvent* e)
{
	if(e == NULL) return;

	RS_Vector mouse = e->getCoordinate();

	switch (getStatus())
	{
	case SetStartPoint:
		undoBuffer.clear();
		if(!spline.get())
		{
			spline.reset(new LC_SplinePoints(container, *data));
			spline->addPoint(mouse);
			preview->addEntity(new RS_Point(preview.get(), RS_PointData(mouse)));
		}
		setStatus(SetNextPoint);
		graphicView->moveRelativeZero(mouse);
		updateMouseButtonHints();
		break;
	case SetNextPoint:
		graphicView->moveRelativeZero(mouse);
		if(spline.get())
		{
			spline->addPoint(mouse);
			drawPreview();
			drawSnapper();
		}
		updateMouseButtonHints();
		break;
	default:
		break;
	}
}

void LC_ActionDrawSplinePoints::commandEvent(RS_CommandEvent* e)
{
	QString c = e->getCommand().toLower();

	switch (getStatus())
	{
	case SetStartPoint:
		if(checkCommand("help", c))
		{
			RS_DIALOGFACTORY->commandMessage(msgAvailableCommands()
				+ getAvailableCommands().join(", "));
			return;
		}
		break;
	case SetNextPoint:
		if (checkCommand("undo", c))
		{
			undo();
			updateMouseButtonHints();
			return;
		}
		if (checkCommand("redo", c))
		{
			redo();
			updateMouseButtonHints();
			return;
		}
		break;
	default:
		break;
	}
}

QStringList LC_ActionDrawSplinePoints::getAvailableCommands()
{
	QStringList cmd;

	switch (getStatus())
	{
	case SetStartPoint:
		break;
	case SetNextPoint:
		if(data->splinePoints.size() > 0)
		{
			cmd += command("undo");
		}
		if(undoBuffer.size() > 0)
		{
			cmd += command("redo");
		}
		if(data->splinePoints.size() > 2)
		{
			cmd += command("close");
		}
		break;
	default:
		break;
	}

	return cmd;
}

void LC_ActionDrawSplinePoints::updateMouseButtonHints()
{
	switch (getStatus())
	{
	case SetStartPoint:
		RS_DIALOGFACTORY->updateMouseWidget(tr("Specify first control point"),
			tr("Cancel"));
		break;
	case SetNextPoint:
		{
		QString msg = "";

		if(data->splinePoints.size() > 2)
		{
			msg += RS_COMMANDS->command("close");
			msg += "/";
		}
		if(data->splinePoints.size() > 0)
		{
			msg += RS_COMMANDS->command("undo");
		}
		if(undoBuffer.size() > 0)
		{
			msg += RS_COMMANDS->command("redo");
		}

		if(data->splinePoints.size() > 0)
		{
			RS_DIALOGFACTORY->updateMouseWidget(
				tr("Specify next control point or [%1]").arg(msg),
				tr("Back"));
		}
		else
		{
			RS_DIALOGFACTORY->updateMouseWidget(
				tr("Specify next control point"),
				tr("Back"));
		}
		}
		break;
	default:
		RS_DIALOGFACTORY->updateMouseWidget();
		break;
	}
}

void LC_ActionDrawSplinePoints::showOptions()
{
	RS_ActionInterface::showOptions();
	RS_DIALOGFACTORY->requestOptions(this, true);
}

void LC_ActionDrawSplinePoints::hideOptions()
{
	RS_ActionInterface::hideOptions();
	RS_DIALOGFACTORY->requestOptions(this, false);
}

void LC_ActionDrawSplinePoints::updateMouseCursor()
{
	graphicView->setMouseCursor(RS2::CadCursor);
}

/*
void RS_ActionDrawSplinePoints::close() {
	if (history.size()>2 && start.valid) {
        //data.endpoint = start;
        //trigger();
                if (spline) {
                        RS_CoordinateEvent e(spline->getStartpoint());
                        coordinateEvent(&e);
                }
                trigger();
        setStatus(SetStartpoint);
        graphicView->moveRelativeZero(start);
    } else {
        RS_DIALOGFACTORY->commandMessage(
            tr("Cannot close sequence of lines: "
               "Not enough entities defined yet."));
    }
}
*/

void LC_ActionDrawSplinePoints::undo()
{
	if(!spline.get())
	{
		RS_DIALOGFACTORY->commandMessage(
			tr("Cannot undo: Not enough entities defined yet."));
		return;
	}

	auto& splinePts = spline->getData().splinePoints;

	size_t nPoints = splinePts.size();
	if(nPoints > 1)
	{
		RS_Vector v = splinePts.back();
		undoBuffer.push_back(v);
		spline->removeLastPoint();

		if(!splinePts.size()) setStatus(SetStartPoint);
		else
		{
			v = splinePts.back();
			graphicView->moveRelativeZero(v);
		}
		graphicView->redraw(RS2::RedrawDrawing);
		drawPreview();
	}
	else
	{
		RS_DIALOGFACTORY->commandMessage(
			tr("Cannot undo: Not enough entities defined yet."));
	}
}

void LC_ActionDrawSplinePoints::redo()
{
	int iBufLen = undoBuffer.size();
	if(iBufLen > 1)
	{
		RS_Vector v = undoBuffer.back();
		spline->addPoint(v);
		undoBuffer.pop_back();

		setStatus(SetNextPoint);
		v = data->splinePoints.back();
		graphicView->moveRelativeZero(v);
		graphicView->redraw(RS2::RedrawDrawing);
	}
	else
	{
		RS_DIALOGFACTORY->commandMessage(
			tr("Cannot undo: Nothing could be redone."));
	}
}

void LC_ActionDrawSplinePoints::setClosed(bool c)
{
	if(data.get()) data->closed = c;
	if(spline.get())
	{
		spline->setClosed(c);
	}
}

bool LC_ActionDrawSplinePoints::isClosed()
{
	if(data.get()) return data->closed;
	return false;
}

// EOF

