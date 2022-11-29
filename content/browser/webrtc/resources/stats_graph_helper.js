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

import {$} from 'chrome://resources/js/util_ts.js';

import {TimelineDataSeries} from './data_series.js';
import {peerConnectionDataStore} from './dump_creator.js';
import {GetSsrcFromReport} from './ssrc_info_manager.js';
import {generateStatsLabel} from './stats_helper.js';
import {TimelineGraphView} from './timeline_graph_view.js';

const STATS_GRAPH_CONTAINER_HEADING_CLASS = 'stats-graph-container-heading';

const RECEIVED_PROPAGATION_DELTA_LABEL =
    'googReceivedPacketGroupPropagationDeltaDebug';
const RECEIVED_PACKET_GROUP_ARRIVAL_TIME_LABEL =
    'googReceivedPacketGroupArrivalTimeDebug';

// Specifies which stats should be drawn on the 'bweCompound' graph and how.
const bweCompoundGraphConfig = {
  googAvailableSendBandwidth: {color: 'red'},
  googTargetEncBitrateCorrected: {color: 'purple'},
  googActualEncBitrate: {color: 'orange'},
  googRetransmitBitrate: {color: 'blue'},
  googTransmitBitrate: {color: 'green'},
};

// Converts the last entry of |srcDataSeries| from the total amount to the
// amount per second.
const totalToPerSecond = function(srcDataSeries) {
  const length = srcDataSeries.dataPoints_.length;
  if (length >= 2) {
    const lastDataPoint = srcDataSeries.dataPoints_[length - 1];
    const secondLastDataPoint = srcDataSeries.dataPoints_[length - 2];
    return Math.floor(
        (lastDataPoint.value - secondLastDataPoint.value) * 1000 /
        (lastDataPoint.time - secondLastDataPoint.time));
  }

  return 0;
};

// Converts the value of total bytes to bits per second.
const totalBytesToBitsPerSecond = function(srcDataSeries) {
  return totalToPerSecond(srcDataSeries) * 8;
};

// Specifies which stats should be converted before drawn and how.
// |convertedName| is the name of the converted value, |convertFunction|
// is the function used to calculate the new converted value based on the
// original dataSeries.
const dataConversionConfig = {
  packetsSent: {
    convertedName: 'packetsSentPerSecond',
    convertFunction: totalToPerSecond,
  },
  bytesSent: {
    convertedName: 'bitsSentPerSecond',
    convertFunction: totalBytesToBitsPerSecond,
  },
  packetsReceived: {
    convertedName: 'packetsReceivedPerSecond',
    convertFunction: totalToPerSecond,
  },
  bytesReceived: {
    convertedName: 'bitsReceivedPerSecond',
    convertFunction: totalBytesToBitsPerSecond,
  },
  // This is due to a bug of wrong units reported for googTargetEncBitrate.
  // TODO (jiayl): remove this when the unit bug is fixed.
  googTargetEncBitrate: {
    convertedName: 'googTargetEncBitrateCorrected',
    convertFunction(srcDataSeries) {
      const length = srcDataSeries.dataPoints_.length;
      const lastDataPoint = srcDataSeries.dataPoints_[length - 1];
      if (lastDataPoint.value < 5000) {
        return lastDataPoint.value * 1000;
      }
      return lastDataPoint.value;
    }
  }
};


// The object contains the stats names that should not be added to the graph,
// even if they are numbers.
const statsNameBlockList = {
  'ssrc': true,
  'googTrackId': true,
  'googComponent': true,
  'googLocalAddress': true,
  'googRemoteAddress': true,
  'googFingerprint': true,
};

function isStandardReportBlocklisted(report) {
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

function isStandardStatBlocklisted(report, statName) {
  // The datachannelid is an identifier, but because it is a number it shows up
  // as a graph if we don't blocklist it.
  if (report.type === 'data-channel' && statName === 'datachannelid') {
    return true;
  }
  // The priority does not change over time on its own; plotting uninteresting.
  if (report.type === 'candidate-pair' && statName === 'priority') {
    return true;
  }
  // The mid/rid associated with a sender/receiver does not change over time;
  // plotting uninteresting.
  if (['inbound-rtp', 'outbound-rtp'].includes(report.type) &&
      ['mid', 'rid'].includes(statName)) {
    return true;
  }
  return false;
}

const graphViews = {};
// Export on |window| since tests access this directly from C++.
window.graphViews = graphViews;
const graphElementsByPeerConnectionId = new Map();

// Returns number parsed from |value|, or NaN if the stats name is blocklisted.
function getNumberFromValue(name, value) {
  if (statsNameBlockList[name]) {
    return NaN;
  }
  if (isNaN(value)) {
    return NaN;
  }
  return parseFloat(value);
}

// Adds the stats report |report| to the timeline graph for the given
// |peerConnectionElement|.
export function drawSingleReport(
    peerConnectionElement, report, isLegacyReport) {
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
    // Propagation deltas are handled separately.
    if (rawLabel === RECEIVED_PROPAGATION_DELTA_LABEL) {
      drawReceivedPropagationDelta(
          peerConnectionElement, report, stats.values[i + 1]);
      continue;
    }
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
    // We need to convert the value if dataConversionConfig[rawLabel] exists.
    if (isLegacyReport && dataConversionConfig[rawLabel]) {
      // Updates the original dataSeries before the conversion.
      addDataSeriesPoints(
          peerConnectionElement, reportType, rawDataSeriesId, rawLabel,
          [stats.timestamp], [rawValue]);

      // Convert to another value to draw on graph, using the original
      // dataSeries as input.
      finalValue = dataConversionConfig[rawLabel].convertFunction(
          peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
              rawDataSeriesId));
      finalLabel = dataConversionConfig[rawLabel].convertedName;
      finalDataSeriesId = reportId + '-' + finalLabel;
    }

    // Updates the final dataSeries to draw.
    addDataSeriesPoints(
        peerConnectionElement, reportType, finalDataSeriesId, finalLabel,
        [stats.timestamp], [finalValue]);

    if (!isLegacyReport &&
        (isStandardReportBlocklisted(report) ||
         isStandardStatBlocklisted(report, rawLabel))) {
      // We do not want to draw certain standard reports but still want to
      // record them in the data series.
      continue;
    }

    // Updates the graph.
    const graphType =
        bweCompoundGraphConfig[finalLabel] ? 'bweCompound' : finalLabel;
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
    if (bweCompoundGraphConfig[label]) {
      dataSeries.setColor(bweCompoundGraphConfig[label].color);
    }
  }
  for (let i = 0; i < times.length; ++i) {
    dataSeries.addPoint(times[i], values[i]);
  }
}

// Draws the received propagation deltas using the packet group arrival time as
// the x-axis. For example, |report.stats.values| should be like
// ['googReceivedPacketGroupArrivalTimeDebug', '[123456, 234455, 344566]',
//  'googReceivedPacketGroupPropagationDeltaDebug', '[23, 45, 56]', ...].
function drawReceivedPropagationDelta(peerConnectionElement, report, deltas) {
  const reportId = report.id;
  const stats = report.stats;
  let times = null;
  // Find the packet group arrival times.
  for (let i = 0; i < stats.values.length - 1; i = i + 2) {
    if (stats.values[i] === RECEIVED_PACKET_GROUP_ARRIVAL_TIME_LABEL) {
      times = stats.values[i + 1];
      break;
    }
  }
  // Unexpected.
  if (times == null) {
    return;
  }

  // Convert |deltas| and |times| from strings to arrays of numbers.
  try {
    deltas = JSON.parse(deltas);
    times = JSON.parse(times);
  } catch (e) {
    console.log(e);
    return;
  }

  // Update the data series.
  const dataSeriesId = reportId + '-' + RECEIVED_PROPAGATION_DELTA_LABEL;
  addDataSeriesPoints(
      peerConnectionElement, 'test type', dataSeriesId,
      RECEIVED_PROPAGATION_DELTA_LABEL, times, deltas);
  // Update the graph.
  const graphViewId = peerConnectionElement.id + '-' + reportId + '-' +
      RECEIVED_PROPAGATION_DELTA_LABEL;
  const date = new Date(times[times.length - 1]);
  if (!graphViews[graphViewId]) {
    graphViews[graphViewId] = createStatsGraphView(
        peerConnectionElement, report, RECEIVED_PROPAGATION_DELTA_LABEL);
    graphViews[graphViewId].setScale(10);
    graphViews[graphViewId].setDateRange(date, date);
    const dataSeries =
        peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
            dataSeriesId);
    graphViews[graphViewId].addDataSeries(dataSeries);
  }
  graphViews[graphViewId].updateEndDate(date);
}

// Get report types for SSRC reports. Returns 'audio' or 'video' where this type
// can be deduced from existing stats labels. Otherwise empty string for
// non-SSRC reports or where type (audio/video) can't be deduced.
function getSsrcReportType(report) {
  if (report.type !== 'ssrc') {
    return '';
  }
  if (report.stats && report.stats.values) {
    // Known stats keys for audio send/receive streams.
    if (report.stats.values.indexOf('audioOutputLevel') !== -1 ||
        report.stats.values.indexOf('audioInputLevel') !== -1) {
      return 'audio';
    }
    // Known stats keys for video send/receive streams.
    // TODO(pbos): Change to use some non-goog-prefixed stats when available for
    // video.
    if (report.stats.values.indexOf('googFrameRateReceived') !== -1 ||
        report.stats.values.indexOf('googFrameRateSent') !== -1) {
      return 'video';
    }
  }
  return '';
}

// Ensures a div container to hold all stats graphs for one track is created as
// a child of |peerConnectionElement|.
function ensureStatsGraphTopContainer(peerConnectionElement, report) {
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

    peerConnectionElement.appendChild(container);
    container.appendChild($('summary-span-template').content.cloneNode(true));
    container.firstChild.firstChild.className =
        STATS_GRAPH_CONTAINER_HEADING_CLASS;
    container.firstChild.firstChild.textContent =
        'Stats graphs for ' + generateStatsLabel(report);
    const statsType = getSsrcReportType(report);
    if (statsType !== '') {
      container.firstChild.firstChild.textContent += ' (' + statsType + ')';
    }

    if (report.type === 'ssrc') {
      const ssrcInfoElement = document.createElement('div');
      container.firstChild.appendChild(ssrcInfoElement);
      ssrcInfoManager.populateSsrcInfo(
          ssrcInfoElement, GetSsrcFromReport(report));
    }
  }
  return container;
}

// Creates the container elements holding a timeline graph
// and the TimelineGraphView object.
function createStatsGraphView(peerConnectionElement, report, statsName) {
  const topContainer =
      ensureStatsGraphTopContainer(peerConnectionElement, report);

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
  if (statsName === 'bweCompound') {
    // Disable getElementById restriction here, since |divId| is not always
    // a valid selector.
    // eslint-disable-next-line no-restricted-properties
    const div = document.getElementById(divId);
    container.insertBefore(
        createBweCompoundLegend(peerConnectionElement, report.id), div);
  }
  return new TimelineGraphView(divId, canvasId);
}

// Creates the legend section for the bweCompound graph.
// Returns the legend element.
function createBweCompoundLegend(peerConnectionElement, reportId) {
  const legend = document.createElement('div');
  for (const prop in bweCompoundGraphConfig) {
    const div = document.createElement('div');
    legend.appendChild(div);
    div.appendChild($('checkbox-template').content.cloneNode(true));
    div.appendChild(document.createTextNode(prop));
    div.style.color = bweCompoundGraphConfig[prop].color;
    div.dataSeriesId = reportId + '-' + prop;
    div.graphViewId =
        peerConnectionElement.id + '-' + reportId + '-bweCompound';
    div.firstChild.addEventListener('click', event => {
      const target =
          peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
              event.target.parentNode.dataSeriesId);
      target.show(event.target.checked);
      graphViews[event.target.parentNode.graphViewId].repaint();
    });
  }
  return legend;
}
