// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// This file contains helper methods to draw the stats timeline graphs.
// Each graph represents a series of stats report for a PeerConnection,
// e.g. 1234-0-ssrc-abcd123-bytesSent is the graph for the series of bytesSent
// for ssrc-abcd123 of PeerConnection 0 in process 1234.
// The graphs are drawn as CANVAS, grouped per report type per PeerConnection.
// Each group has an expand/collapse button and is collapsed initially.
//

import {$} from 'chrome://resources/js/util.js';

import {TimelineDataSeries} from './data_series.js';
import {peerConnectionDataStore} from './dump_creator.js';
import {generateStatsLabel} from './stats_helper.js';
import {TimelineGraphView} from './timeline_graph_view.js';

const STATS_GRAPH_CONTAINER_HEADING_CLASS = 'stats-graph-container-heading';

function isReportBlocklisted(report) {
  // Codec stats reflect what has been negotiated. They don't contain
  // information that is useful in graphs.
  if (report.type === 'codec') {
    return true;
  }
  // Unused data channels can stay in "connecting" indefinitely and their
  // counters stay zero.
  if (report.type === 'data-channel' &&
      readReportStat(report, 'state') === 'connecting') {
    return true;
  }
  // The same is true for transports and "new".
  if (report.type === 'transport' &&
      readReportStat(report, 'dtlsState') === 'new') {
    return true;
  }
  // Local and remote candidates don't change over time and there are several of
  // them.
  if (report.type === 'local-candidate' || report.type === 'remote-candidate') {
    return true;
  }
  return false;
}

function readReportStat(report, stat) {
  const values = report.stats.values;
  for (let i = 0; i < values.length; i += 2) {
    if (values[i] === stat) {
      return values[i + 1];
    }
  }
  return undefined;
}

function isStatBlocklisted(report, statName) {
  // The priority does not change over time on its own; plotting uninteresting.
  if (report.type === 'candidate-pair' && statName === 'priority') {
    return true;
  }
  // The mid/rid and ssrcs associated with a sender/receiver do not change
  // over time; plotting uninteresting.
  if (['inbound-rtp', 'outbound-rtp',
        'remote-inbound-rtp', 'remote-outbound-rtp'].includes(report.type) &&
      ['mid', 'rid', 'ssrc', 'rtxSsrc', 'fecSsrc'].includes(statName)) {
    return true;
  }
  // Last packet sent/received timestamps on candidate-pair and inbound-rtp
  // do not plot nicely.
  if (['candidate-pair', 'inbound-rtp'].includes(report.type) &&
      ['lastPacketSentTimestamp',
        'lastPacketReceivedTimestamp'].includes(statName)) {
    return true;
  }
  return false;
}

const graphViews = {};
// Export on |window| since tests access this directly from C++.
window.graphViews = graphViews;
const graphElementsByPeerConnectionId = new Map();

// Returns number parsed from |value|, or NaN.
function getNumberFromValue(name, value) {
  if (isNaN(value)) {
    return NaN;
  }
  return parseFloat(value);
}

// Adds the stats report |report| to the timeline graph for the given
// |peerConnectionElement|.
export function drawSingleReport(
    peerConnectionElement, report) {
  const reportType = report.type;
  const reportId = report.id;
  const stats = report.stats;
  if (!stats || !stats.values) {
    return;
  }

  const childrenBefore = peerConnectionElement.hasChildNodes() ?
      Array.from(peerConnectionElement.childNodes) :
      [];

  for (let i = 0; i < stats.values.length - 1; i = i + 2) {
    const rawLabel = stats.values[i];
    const rawDataSeriesId = reportId + '-' + rawLabel;
    const rawValue = getNumberFromValue(rawLabel, stats.values[i + 1]);
    if (isNaN(rawValue)) {
      // We do not draw non-numerical values, but still want to record it in the
      // data series.
      addDataSeriesPoints(
          peerConnectionElement, reportType, rawDataSeriesId, rawLabel,
          [stats.timestamp], [stats.values[i + 1]]);
      continue;
    }
    let finalDataSeriesId = rawDataSeriesId;
    let finalLabel = rawLabel;
    let finalValue = rawValue;

    // Updates the final dataSeries to draw.
    addDataSeriesPoints(
        peerConnectionElement, reportType, finalDataSeriesId, finalLabel,
        [stats.timestamp], [finalValue]);

    if (isReportBlocklisted(report) || isStatBlocklisted(report, rawLabel)) {
      // We do not want to draw certain reports but still want to
      // record them in the data series.
      continue;
    }

    // Updates the graph.
    const graphType = finalLabel;
    const graphViewId =
        peerConnectionElement.id + '-' + reportId + '-' + graphType;

    if (!graphViews[graphViewId]) {
      graphViews[graphViewId] =
          createStatsGraphView(peerConnectionElement, report, graphType);
      const searchParameters = new URLSearchParams(window.location.search);
      if (searchParameters.has('statsInterval')) {
        const statsInterval = Math.max(
            parseInt(searchParameters.get('statsInterval'), 10),
            100);
        if (isFinite(statsInterval)) {
          graphViews[graphViewId].setScale(statsInterval);
        }
      }
      const date = new Date(stats.timestamp);
      graphViews[graphViewId].setDateRange(date, date);
    }
    // Ensures the stats graph title is up-to-date.
    ensureStatsGraphContainer(peerConnectionElement, report);
    // Adds the new dataSeries to the graphView. We have to do it here to cover
    // both the simple and compound graph cases.
    const dataSeries =
        peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
            finalDataSeriesId);
    if (!graphViews[graphViewId].hasDataSeries(dataSeries)) {
      graphViews[graphViewId].addDataSeries(dataSeries);
    }
    graphViews[graphViewId].updateEndDate();
  }
  // Add a synthetic data series for the timestamp.
  addDataSeriesPoints(
    peerConnectionElement, reportType, reportId + '-timestamp',
    reportId + '-timestamp', [stats.timestamp], [stats.timestamp]);

  const childrenAfter = peerConnectionElement.hasChildNodes() ?
      Array.from(peerConnectionElement.childNodes) :
      [];
  for (let i = 0; i < childrenAfter.length; ++i) {
    if (!childrenBefore.includes(childrenAfter[i])) {
      let graphElements =
          graphElementsByPeerConnectionId.get(peerConnectionElement.id);
      if (!graphElements) {
        graphElements = [];
        graphElementsByPeerConnectionId.set(
            peerConnectionElement.id, graphElements);
      }
      graphElements.push(childrenAfter[i]);
    }
  }
}

export function removeStatsReportGraphs(peerConnectionElement) {
  const graphElements =
      graphElementsByPeerConnectionId.get(peerConnectionElement.id);
  if (graphElements) {
    for (let i = 0; i < graphElements.length; ++i) {
      peerConnectionElement.removeChild(graphElements[i]);
    }
    graphElementsByPeerConnectionId.delete(peerConnectionElement.id);
  }
  Object.keys(graphViews).forEach(key => {
    if (key.startsWith(peerConnectionElement.id)) {
      delete graphViews[key];
    }
  });
}

// Makes sure the TimelineDataSeries with id |dataSeriesId| is created,
// and adds the new data points to it. |times| is the list of timestamps for
// each data point, and |values| is the list of the data point values.
function addDataSeriesPoints(
    peerConnectionElement, reportType, dataSeriesId, label, times, values) {
  let dataSeries =
      peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
          dataSeriesId);
  if (!dataSeries) {
    dataSeries = new TimelineDataSeries(reportType);
    peerConnectionDataStore[peerConnectionElement.id].setDataSeries(
        dataSeriesId, dataSeries);
  }
  for (let i = 0; i < times.length; ++i) {
    dataSeries.addPoint(times[i], values[i]);
  }
}

// Ensures a div container to the stats graph for a peerConnectionElement is
// created as a child of the |peerConnectionElement|.
function ensureStatsGraphTopContainer(peerConnectionElement) {
  const containerId = peerConnectionElement.id + '-graph-container';
  let container = document.getElementById(containerId);
  if (!container) {
    container = document.createElement('div');
    container.id = containerId;
    container.className = 'stats-graph-container';
    const label = document.createElement('label');
    label.innerText = 'Filter statistics graphs by type including ';
    container.appendChild(label);
    const input = document.createElement('input');
    input.placeholder = 'separate multiple values by `,`';
    input.size = 25;
    input.oninput = (e) => filterStats(e, container);
    container.appendChild(input);

    peerConnectionElement.appendChild(container);
  }
  return container;
}

// Ensures a div container to the stats graph for a single set of data is
// created as a child of the |peerConnectionElement|'s graph container.
function ensureStatsGraphContainer(peerConnectionElement, report) {
  const topContainer = ensureStatsGraphTopContainer(peerConnectionElement);
  const containerId = peerConnectionElement.id + '-' + report.type + '-' +
      report.id + '-graph-container';
  // Disable getElementById restriction here, since |containerId| is not always
  // a valid selector.
  // eslint-disable-next-line no-restricted-properties
  let container = document.getElementById(containerId);
  if (!container) {
    container = document.createElement('details');
    container.id = containerId;
    container.className = 'stats-graph-container';
    container.attributes['data-statsType'] = report.type;

    peerConnectionElement.appendChild(container);
    container.appendChild($('summary-span-template').content.cloneNode(true));
    container.firstChild.firstChild.className =
        STATS_GRAPH_CONTAINER_HEADING_CLASS;
    topContainer.appendChild(container);
  }
  // Update the label all the time to account for new information.
  container.firstChild.firstChild.textContent = 'Stats graphs for ' +
    generateStatsLabel(report);
  return container;
}

// Creates the container elements holding a timeline graph
// and the TimelineGraphView object.
function createStatsGraphView(peerConnectionElement, report, statsName) {
  const topContainer =
      ensureStatsGraphContainer(peerConnectionElement, report);

  const graphViewId =
      peerConnectionElement.id + '-' + report.id + '-' + statsName;
  const divId = graphViewId + '-div';
  const canvasId = graphViewId + '-canvas';
  const container = document.createElement('div');
  container.className = 'stats-graph-sub-container';

  topContainer.appendChild(container);
  const canvasDiv = $('container-template').content.cloneNode(true);
  canvasDiv.querySelectorAll('div')[0].textContent = statsName;
  canvasDiv.querySelectorAll('div')[1].id = divId;
  canvasDiv.querySelector('canvas').id = canvasId;
  container.appendChild(canvasDiv);
  return new TimelineGraphView(divId, canvasId);
}

/**
 * Apply a filter to the stats graphs
 * @param event InputEvent from the filter input field.
 * @param container stats table container element.
 * @private
 */
function filterStats(event, container) {
  const filter =  event.target.value;
  const filters = filter.split(',');
  container.childNodes.forEach(node => {
    if (node.nodeName !== 'DETAILS') {
      return;
    }
    const statsType = node.attributes['data-statsType'];
    if (!filter || filters.includes(statsType) ||
        filters.find(f => statsType.includes(f))) {
      node.style.display = 'block';
    } else {
      node.style.display = 'none';
    }
  });
}
