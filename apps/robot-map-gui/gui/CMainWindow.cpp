/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2017, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */
#include "CMainWindow.h"
#include "CGLWidget.h"
#include "CDocument.h"
#include "observationTree/CObservationTreeModel.h"
#include "observationTree/CPosesNode.h"
#include "observationTree/CRangeScanNode.h"
#include "observationTree/CObservationImageNode.h"
#include "observationTree/CObservationsNode.h"
#include "observationTree/CPairNode.h"

#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <QTreeWidgetItem>
#include <QDebug>

#include "ui_CMainWindow.h"


CMainWindow::CMainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_document(nullptr)
	, m_model(nullptr)
	, m_ui(std::make_unique<Ui::CMainWindow>())
{
	m_ui->setupUi(this);
	QObject::connect(m_ui->openAction,		SIGNAL(triggered(bool)),			SLOT(openMap()));
	QObject::connect(m_ui->m_configWidget, SIGNAL(openedConfig(const std::string)), SLOT(updateConfig(const std::string)));
	QObject::connect(m_ui->m_configWidget, SIGNAL(applyConfigurationForCurrentMaps()), SLOT(applyConfigurationForCurrentMaps()));
	QObject::connect(m_ui->m_configWidget, SIGNAL(addedMap()), SLOT(updateConfig()));
	QObject::connect(m_ui->m_configWidget, SIGNAL(removedMap()), SLOT(updateConfig()));
	QObject::connect(m_ui->m_configWidget, SIGNAL(updatedConfig()), SLOT(updateConfig()));
	QObject::connect(m_ui->m_observationsTree,	SIGNAL(clicked(const QModelIndex &)),	SLOT(itemClicked(const QModelIndex &)));
	QObject::connect(m_ui->m_actionLoadConfig,	SIGNAL(triggered(bool)), m_ui->m_configWidget, SLOT(openConfig()));

	QObject::connect(m_ui->m_actionShowAllObs, SIGNAL(triggered(bool)), SLOT(showAllObservation(bool)));

	QObject::connect(m_ui->m_zoom, SIGNAL(valueChanged(double)), SLOT(zoomChanged(double)));
	QObject::connect(m_ui->m_zoomSlider, SIGNAL(valueChanged(int)), SLOT(zoomChanged(int)));
	QObject::connect(m_ui->m_tabWidget, SIGNAL(currentChanged(int)), SLOT(updateZoomInfo(int)));
}

CMainWindow::~CMainWindow()
{
	if (m_document)
		delete m_document;

	if (m_model)
		delete m_model;

	delete m_ui->m_configWidget;
}

void CMainWindow::updateZoomInfo(int index)
{
	if (m_ui->m_tabWidget->count())
	{
		CGlWidget *gl = dynamic_cast<CGlWidget *>(m_ui->m_tabWidget->widget(index));
		assert(gl);

		changeZoomInfo(gl->getZoom());
	}
}

void CMainWindow::changeZoomInfo(float zoom)
{
	int zoomInt = zoom;
	m_ui->m_zoomSlider->setValue(zoomInt);

	double zoomDouble = zoom;
	m_ui->m_zoom->setValue(zoomDouble);
}

void CMainWindow::zoomChanged(double d)
{
	float zoomFloat = d;
	int zoomInt = zoomFloat;
	m_ui->m_zoomSlider->setValue(zoomInt);
	CGlWidget *gl = dynamic_cast<CGlWidget *>(m_ui->m_tabWidget->currentWidget());
	assert(gl);
	gl->setZoom(zoomFloat);
}

void CMainWindow::zoomChanged(int d)
{
	double zoomD = d;
	float zoomFloat = zoomD;
	m_ui->m_zoom->setValue(zoomD);
	CGlWidget *gl = dynamic_cast<CGlWidget *>(m_ui->m_tabWidget->currentWidget());
	assert(gl);
	gl->setZoom(zoomFloat);
}

void CMainWindow::showAllObservation(bool is)
{
	for (int i = 0; i < m_ui->m_tabWidget->count(); ++i)
	{
		CGlWidget *gl = dynamic_cast<CGlWidget *>(m_ui->m_tabWidget->widget(i));
		assert(gl);
		gl->setSelectedObservation(is);
	}
}

void CMainWindow::openMap()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Files (*.simplemap *.simplemap.gz)"));

	if (fileName.size() == 0)
		return;

	createNewDocument();

	try
	{
		m_document->loadSimpleMap(fileName.toStdString());

		mrpt::maps::TSetOfMetricMapInitializers mapCfg = m_ui->m_configWidget->config();
		m_document->setListOfMaps(mapCfg);

		updateRenderMapFromConfig();

		if (m_model)
			delete m_model;
		m_model = new CObservationTreeModel(m_document->simplemap(), m_ui->m_observationsTree);
		m_ui->m_observationsTree->setModel(m_model);
	}
	catch(std::exception &)
	{
		createNewDocument();
		qDebug() << "Unexpected runtime error!";
	}
}

void CMainWindow::itemClicked(const QModelIndex &index)
{
	CNode* node = m_model->getNodeFromIndexSafe(index);
	CNode::ObjectType type = node->type();

	switch (type) {
	case CNode::ObjectType::RangeScan:
	{
		CRangeScanNode *obsNode = dynamic_cast<CRangeScanNode *>(node);
		assert(obsNode);

		mrpt::opengl::CPlanarLaserScan::Ptr obj = mrpt::opengl::CPlanarLaserScan::Create();
		obj->setScan(*(obsNode->observation().get()));
		obj->setPose( obsNode->getPose() );
		obj->setSurfaceColor(1.0f,0.0f,0.0f, 0.5f);

		for (int i = 0; i < m_ui->m_tabWidget->count(); ++i)
		{
			QWidget *w = m_ui->m_tabWidget->widget(i);
			CGlWidget *gl = dynamic_cast<CGlWidget *>(w);
			assert(gl);
			gl->setSelected(obsNode->getPose());
			gl->setLaserScan(obj);
		}
		break;
	}
	case CNode::ObjectType::Pos:
	{
		CPoseNode *posesNode = dynamic_cast<CPoseNode *>(node);
		assert(posesNode);

		for (int i = 0; i < m_ui->m_tabWidget->count(); ++i)
		{
			QWidget *w = m_ui->m_tabWidget->widget(i);
			CGlWidget *gl = dynamic_cast<CGlWidget *>(w);
			assert(gl);
			gl->setSelected(posesNode->getPose());
		}
		break;
	}
	default:
		break;
	}
}

void CMainWindow::updateConfig()
{
	if (!m_document)
		return;
	mrpt::maps::TSetOfMetricMapInitializers mapCfg = m_ui->m_configWidget->config();
	m_document->setListOfMaps(mapCfg);
	updateRenderMapFromConfig();
}

void CMainWindow::updateConfig(const std::string str)
{
	if (!m_document)
		return;

	m_document->setConfig(str);

	auto config = m_document->config();
	m_ui->m_configWidget->setConfig(config);
	updateRenderMapFromConfig();
}

void CMainWindow::applyConfigurationForCurrentMaps()
{
	if (!m_document)
		return;

	mrpt::maps::TSetOfMetricMapInitializers mapCfg = m_ui->m_configWidget->config();
	m_document->setListOfMaps(mapCfg);

	auto renderizableMaps = m_document->renderizableMaps();
	for (int i = 0; i < m_ui->m_tabWidget->count(); ++i)
	{
		std::string name = m_ui->m_tabWidget->tabText(i).toStdString();
		auto it = renderizableMaps.find(name);
		assert(it != renderizableMaps.end());

		CGlWidget *gl = dynamic_cast<CGlWidget *>(m_ui->m_tabWidget->widget(i));
		assert(gl);

		gl->fillMap(it->second);
	}
}


void CMainWindow::updateRenderMapFromConfig()
{
	for (int i = 0; i < m_ui->m_tabWidget->count(); ++i)
	{
		QWidget *w = m_ui->m_tabWidget->widget(i);
		delete w;
	}

	m_ui->m_tabWidget->clear();

	auto renderizableMaps = m_document->renderizableMaps();
	for(auto &it: renderizableMaps)
	{
		CGlWidget *gl = new CGlWidget();
		gl->fillMap(it.second);
		m_ui->m_tabWidget->addTab(gl, QString::fromStdString(it.first));
		qDebug() << QString::fromStdString(it.first);

		gl->setDocument(m_document);
		gl->setSelectedObservation(m_ui->m_actionShowAllObs->isChecked());
		QObject::connect(gl, SIGNAL(zoomChanged(float)), SLOT(changeZoomInfo(float)));
	}
}

void CMainWindow::createNewDocument()
{
	if (m_document)
		delete m_document;

	m_document = new CDocument();

	for (int i = 0; i < m_ui->m_tabWidget->count(); ++i)
	{
		CGlWidget *gl = dynamic_cast<CGlWidget *>(m_ui->m_tabWidget->widget(i));
		assert(gl);

		gl->setDocument(m_document);
	}
}
