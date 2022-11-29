// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util_ts.js';

import {MAX_STATS_DATA_POINT_BUFFER_SIZE} from './data_series.js';
import {DumpCreator, peerConnectionDataStore, userMediaRequests} from './dump_creator.js';
import {PeerConnectionUpdateTable} from './peer_connection_update_table.js';
import {SsrcInfoManager} from './ssrc_info_manager.js';
import {drawSingleReport, removeStatsReportGraphs} from './stats_graph_helper.js';
import {StatsRatesCalculator, StatsReport} from './stats_rates_calculator.js';
import {StatsTable} from './stats_table.js';
import {TabView} from './tab_view.js';
import {createIceCandidateGrid, updateIceCandidateGrid} from './candidate_grid.js';
import {UserMediaTable} from './user_media.js';

const OPTION_GETSTATS_STANDARD = 'Standardized (promise-based) getStats() API';
const OPTION_GETSTATS_LEGACY =
    'Legacy Non-Standard (callback-based) getStats() API';
let currentGetStatsMethod = OPTION_GETSTATS_STANDARD;

let tabView = null;
let ssrcInfoManager = null;
let peerConnectionUpdateTable = null;
let statsTable = null;
let userMediaTable = null;
let dumpCreator = null;

// Exporting these on window since they are directly accessed by tests.
window.setCurrentGetStatsMethod = function(method) {
  currentGetStatsMethod = method;
};
window.OPTION_GETSTATS_LEGACY = OPTION_GETSTATS_LEGACY;

/** Maps from id (see getPeerConnectionId) to StatsRatesCalculator. */
const statsRatesCalculatorById = new Map();

/** A simple class to store the updates and stats data for a peer connection. */
  /** @constructor */
class PeerConnectionRecord {
  constructor() {
    /** @private */
    this.record_ = {
      pid: -1,
      constraints: {},
      rtcConfiguration: [],
      stats: {},
      updateLog: [],
      url: '',
    };
  }

  /** @override */
  toJSON() {
    return this.record_;
  }

  /**
   * Adds the initialization info of the peer connection.
   * @param {number} pid The pid of the process hosting the peer connection.
   * @param {string} url The URL of the web page owning the peer connection.
   * @param {Array} rtcConfiguration
   * @param {!Object} constraints Media constraints.
   */
  initialize(pid, url, rtcConfiguration, constraints) {
    this.record_.pid = pid;
    this.record_.url = url;
    this.record_.rtcConfiguration = rtcConfiguration;
    this.record_.constraints = constraints;
  }

  resetStats() {
    this.record_.stats = {};
  }

  /**
   * @param {string} dataSeriesId The TimelineDataSeries identifier.
   * @return {!TimelineDataSeries}
   */
  getDataSeries(dataSeriesId) {
    return this.record_.stats[dataSeriesId];
  }

  /**
   * @param {string} dataSeriesId The TimelineDataSeries identifier.
   * @param {!TimelineDataSeries} dataSeries The TimelineDataSeries to set to.
   */
  setDataSeries(dataSeriesId, dataSeries) {
    this.record_.stats[dataSeriesId] = dataSeries;
  }

  /**
   * @param {!Object} update The object contains keys "time", "type", and
   *   "value".
   */
  addUpdate(update) {
    const time = new Date(parseFloat(update.time));
    this.record_.updateLog.push({
      time: time.toLocaleString(),
      type: update.type,
      value: update.value,
    });
  }
}

function initialize() {
  dumpCreator = new DumpCreator($('content-root'));
  $('content-root').appendChild(createStatsSelectionOptionElements());
  tabView = new TabView($('content-root'));
  ssrcInfoManager = new SsrcInfoManager();
  window.ssrcInfoManager = ssrcInfoManager;
  peerConnectionUpdateTable = new PeerConnectionUpdateTable();
  statsTable = new StatsTable(ssrcInfoManager);
  userMediaTable = new UserMediaTable(tabView, userMediaRequests);

  // Add listeners for all the updates that get sent from webrtc_internals.cc.
  addWebUiListener('add-peer-connection', addPeerConnection);
  addWebUiListener('update-peer-connection', updatePeerConnection);
  addWebUiListener('update-all-peer-connections', updateAllPeerConnections);
  addWebUiListener('remove-peer-connection', removePeerConnection);
  addWebUiListener('add-standard-stats', addStandardStats);
  addWebUiListener('add-legacy-stats', addLegacyStats);
  addWebUiListener('add-media', (data) => {
    userMediaRequests.push(data);
    userMediaTable.addMedia(data)
  });
  addWebUiListener('update-media', (data) => {
    userMediaRequests.push(data);
    userMediaTable.updateMedia(data);
  });
  addWebUiListener('remove-media-for-renderer', (data) => {
    for (let i = userMediaRequests.length - 1; i >= 0; --i) {
      if (userMediaRequests[i].rid === data.rid) {
        userMediaRequests.splice(i, 1);
      }
    }
    userMediaTable.removeMediaForRenderer(data);
  });
  addWebUiListener(
      'event-log-recordings-file-selection-cancelled',
      eventLogRecordingsFileSelectionCancelled);
  addWebUiListener(
      'audio-debug-recordings-file-selection-cancelled',
      audioDebugRecordingsFileSelectionCancelled);

  // Request initial startup parameters.
  sendWithPromise('finishedDOMLoad').then(params => {
    if (params.audioDebugRecordingsEnabled) {
      dumpCreator.setAudioDebugRecordingsCheckbox();
    }
    if (params.eventLogRecordingsEnabled) {
      dumpCreator.setEventLogRecordingsCheckbox();
    }
    dumpCreator.setEventLogRecordingsCheckboxMutability(
        params.eventLogRecordingsToggleable);
  });

  // Requests stats from all peer connections every second unless specified via
  // ?statsInterval=(milliseconds >= 100ms)
  const searchParameters = new URLSearchParams(window.location.search);
  let statsInterval = 1000;
  if (searchParameters.has('statsInterval')) {
    statsInterval = Math.max(
        parseInt(searchParameters.get('statsInterval'), 10),
        100);
    if (!isFinite(statsInterval)) {
      statsInterval = 1000;
    }
  }
  window.setInterval(requestStats, statsInterval);
}
document.addEventListener('DOMContentLoaded', initialize);

function createStatsSelectionOptionElements() {
  const statsElement = $('stats-template').content.cloneNode(true);
  const selectElement = statsElement.getElementById('statsSelectElement');
  const legacyStatsElement = statsElement.getElementById(
      'legacy-stats-warning');
  selectElement.onchange = () => {
    currentGetStatsMethod = selectElement.value;
    legacyStatsElement.style.display =
        currentGetStatsMethod === OPTION_GETSTATS_LEGACY ? 'block' : 'none';
    Object.keys(peerConnectionDataStore).forEach(id => {
      // Disable getElementById restriction here, since |id| is not always
      // a valid selector.
      // eslint-disable-next-line no-restricted-properties
      const peerConnectionElement = document.getElementById(id);
      statsTable.clearStatsLists(peerConnectionElement);
      removeStatsReportGraphs(peerConnectionElement);
      peerConnectionDataStore[id].resetStats();
    });
  };

  [OPTION_GETSTATS_STANDARD, OPTION_GETSTATS_LEGACY].forEach(option => {
    const optionElement = document.createElement('option');
    optionElement.setAttribute('value', option);
    optionElement.appendChild(document.createTextNode(option));
    selectElement.appendChild(optionElement);
  });

  selectElement.value = currentGetStatsMethod;
  return statsElement;
}

function requestStats() {
  if (currentGetStatsMethod === OPTION_GETSTATS_STANDARD) {
    requestStandardStats();
  } else if (currentGetStatsMethod === OPTION_GETSTATS_LEGACY) {
    requestLegacyStats();
  }
}

/**
 * Sends a request to the browser to get peer connection statistics from the
 * standard getStats() API (promise-based).
 */
function requestStandardStats() {
  if (Object.keys(peerConnectionDataStore).length > 0) {
    chrome.send('getStandardStats');
  }
}

/**
 * Sends a request to the browser to get peer connection statistics from the
 * legacy getStats() API (callback-based non-standard API with goog-stats).
 */
function requestLegacyStats() {
  if (Object.keys(peerConnectionDataStore).length > 0) {
    chrome.send('getLegacyStats');
  }
}

/*
 * Change to use the legacy getStats() API instead. This is used for a
 * work-around for https://crbug.com/999136.
 * TODO(https://crbug.com/1004239): Delete this method.
 */
function changeToLegacyGetStats() {
  currentGetStatsMethod = OPTION_GETSTATS_LEGACY;
  const selectElement = $('statsSelectElement');
  selectElement.value = currentGetStatsMethod;
  requestStats();
}
window.changeToLegacyGetStats = changeToLegacyGetStats;

/**
 * A helper function for getting a peer connection element id.
 *
 * @param {!Object<number>} data The object containing the rid and lid of the
 *     peer connection.
 * @return {string} The peer connection element id.
 */
function getPeerConnectionId(data) {
  return data.rid + '-' + data.lid;
}


/**
 * Extracts ssrc info from a setLocal/setRemoteDescription update.
 *
 * @param {!PeerConnectionUpdateEntry} data The peer connection update data.
 */
function extractSsrcInfo(data) {
  if (data.type === 'setLocalDescription' ||
      data.type === 'setRemoteDescription') {
    ssrcInfoManager.addSsrcStreamInfo(data.value);
  }
}


/**
 * A helper function for appending a child element to |parent|.
 *
 * @param {!Element} parent The parent element.
 * @param {string} tag The child element tag.
 * @param {string} text The textContent of the new DIV.
 * @return {!Element} the new DIV element.
 */
function appendChildWithText(parent, tag, text) {
  const child = document.createElement(tag);
  child.textContent = text;
  parent.appendChild(child);
  return child;
}

/**
 * Helper for adding a peer connection update.
 *
 * @param {Element} peerConnectionElement
 * @param {!PeerConnectionUpdateEntry} update The peer connection update data.
 */
function addPeerConnectionUpdate(peerConnectionElement, update) {
  peerConnectionUpdateTable.addPeerConnectionUpdate(
      peerConnectionElement, update);
  extractSsrcInfo(update);
  peerConnectionDataStore[peerConnectionElement.id].addUpdate(update);
}


/** Browser message handlers. */


/**
 * Removes all information about a peer connection.
 *
 * @param {!Object<number>} data The object containing the rid and lid of a peer
 *     connection.
 */
function removePeerConnection(data) {
  // Disable getElementById restriction here, since |getPeerConnectionId| does
  // not return valid selectors.
  // eslint-disable-next-line no-restricted-properties
  const element = document.getElementById(getPeerConnectionId(data));
  if (element) {
    delete peerConnectionDataStore[element.id];
    tabView.removeTab(element.id);
  }
}

/**
 * Adds a peer connection.
 *
 * @param {!Object} data The object containing the rid, lid, pid, url,
 *     rtcConfiguration, and constraints of a peer connection.
 */
function addPeerConnection(data) {
  const id = getPeerConnectionId(data);

  if (!peerConnectionDataStore[id]) {
    peerConnectionDataStore[id] = new PeerConnectionRecord();
  }
  peerConnectionDataStore[id].initialize(
      data.pid, data.url, data.rtcConfiguration, data.constraints);

  // Disable getElementById restriction here, since |id| is not always
  // a valid selector.
  // eslint-disable-next-line no-restricted-properties
  let peerConnectionElement = document.getElementById(id);
  if (!peerConnectionElement) {
    const details = `[ rid: ${data.rid}, lid: ${data.lid}, pid: ${data.pid} ]`;
    peerConnectionElement = tabView.addTab(id, data.url + " " + details);
  }

  const p = document.createElement('p');
  appendChildWithText(p, 'span', data.url);
  appendChildWithText(p, 'span', ', ');
  appendChildWithText(p, 'span', data.rtcConfiguration);
  if (data.constraints !== '') {
    appendChildWithText(p, 'span', ', ');
    appendChildWithText(p, 'span', data.constraints);
  }
  peerConnectionElement.appendChild(p);

  // Show deprecation notices as a list.
  // Note: data.rtcConfiguration is not in JSON format and may
  // not be defined in tests.
  const deprecationNotices = document.createElement('ul');
  if (data.rtcConfiguration) {
    deprecationNotices.className = 'peerconnection-deprecations';
  }
  if (data.constraints) {
    if (data.constraints.indexOf('enableDtlsSrtp:') !== -1) {
      if (data.constraints.indexOf('enableDtlsSrtp: {exact: false}') !== -1) {
        appendChildWithText(deprecationNotices, 'li',
          'The constraint "DtlsSrtpKeyAgreement" will be removed. You have ' +
          'specified a "false" value for this constraint, which is ' +
          'interpreted as an attempt to use the deprecated "SDES" key ' +
          'negotiation method. This functionality will be removed; use a ' +
          'service that supports DTLS key negotiation instead.');
      } else {
        appendChildWithText(deprecationNotices, 'li',
          'The constraint "DtlsSrtpKeyAgreement" will be removed. You have ' +
          'specified a "true" value for this constraint, which has no ' +
          'effect, but you can remove this constraint for tidiness.');
      }
    }
  }
  peerConnectionElement.appendChild(deprecationNotices);

  const iceConnectionStates = document.createElement('div');
  iceConnectionStates.textContent = 'ICE connection state: new';
  iceConnectionStates.className = 'iceconnectionstate';
  peerConnectionElement.appendChild(iceConnectionStates);

  const connectionStates = document.createElement('div');
  connectionStates.textContent = 'Connection state: new';
  connectionStates.className = 'connectionstate';
  peerConnectionElement.appendChild(connectionStates);

  const signalingStates = document.createElement('div');
  signalingStates.textContent = 'Signaling state: new';
  signalingStates.className = 'signalingstate';
  peerConnectionElement.appendChild(signalingStates);

  const candidatePair = document.createElement('div');
  candidatePair.textContent = 'ICE Candidate pair: ';
  candidatePair.className = 'candidatepair';
  candidatePair.appendChild(document.createElement('span'));
  peerConnectionElement.appendChild(candidatePair);

  createIceCandidateGrid(peerConnectionElement);
  return peerConnectionElement;
}


/**
 * Adds a peer connection update.
 *
 * @param {!PeerConnectionUpdateEntry} data The peer connection update data.
 */
function updatePeerConnection(data) {
  // Disable getElementById restriction here, since |getPeerConnectionId| does
  // not return valid selectors.
  const peerConnectionElement =
  // eslint-disable-next-line no-restricted-properties
      document.getElementById(getPeerConnectionId(data));
  addPeerConnectionUpdate(peerConnectionElement, data);
}


/**
 * Adds the information of all peer connections created so far.
 *
 * @param {Array<!Object>} data An array of the information of all peer
 *     connections. Each array item contains rid, lid, pid, url,
 *     rtcConfiguration, constraints, and an array of updates as the log.
 */
function updateAllPeerConnections(data) {
  for (let i = 0; i < data.length; ++i) {
    const peerConnection = addPeerConnection(data[i]);

    const log = data[i].log;
    if (!log) {
      continue;
    }
    for (let j = 0; j < log.length; ++j) {
      addPeerConnectionUpdate(peerConnection, log[j]);
    }
  }
  requestStats();
}

/**
 * Handles the report of stats originating from the standard getStats() API.
 *
 * @param {!Object} data The object containing rid, lid, and reports, where
 *     reports is an array of stats reports. Each report contains id, type,
 *     and stats, where stats is the object containing timestamp and values,
 *     which is an array of strings, whose even index entry is the name of the
 *     stat, and the odd index entry is the value.
 */
function addStandardStats(data) {
  if (currentGetStatsMethod != OPTION_GETSTATS_STANDARD) {
    return;  // Obsolete!
  }

  // Disable getElementById restriction here, since |getPeerConnectionId| does
  // not return valid selectors.
  // eslint-disable-next-line no-restricted-properties
  const peerConnectionElement =
      // eslint-disable-next-line no-restricted-properties
      document.getElementById(getPeerConnectionId(data));
  if (!peerConnectionElement) {
    return;
  }
  const pcId = getPeerConnectionId(data);
  let statsRatesCalculator = statsRatesCalculatorById.get(pcId);
  if (!statsRatesCalculator) {
    statsRatesCalculator = new StatsRatesCalculator();
    statsRatesCalculatorById.set(pcId, statsRatesCalculator);
  }
  const r = StatsReport.fromInternalsReportList(data.reports);
  statsRatesCalculator.addStatsReport(r);
  data.reports = statsRatesCalculator.currentReport.toInternalsReportList();
  for (let i = 0; i < data.reports.length; ++i) {
    const report = data.reports[i];
    statsTable.addStatsReport(peerConnectionElement, report);
    drawSingleReport(peerConnectionElement, report, false);
  }
  // Determine currently connected candidate pair.
  const stats = r.statsById;

  let activeCandidatePair = null;
  let remoteCandidate = null;
  let localCandidate = null;

  // Get the first active candidate pair. This ignores the rare case of
  // non-bundled connections.
  stats.forEach(report => {
    if (report.type === 'transport' && !activeCandidatePair) {
      activeCandidatePair = stats.get(report.selectedCandidatePairId);
    }
  });

  const candidateElement = peerConnectionElement
    .getElementsByClassName('candidatepair')[0].firstElementChild;
  if (activeCandidatePair) {
    if (activeCandidatePair.remoteCandidateId) {
      remoteCandidate = stats.get(activeCandidatePair.remoteCandidateId);
    }
    if (activeCandidatePair.localCandidateId) {
      localCandidate = stats.get(activeCandidatePair.localCandidateId);
    }
    if (localCandidate && localCandidate.address &&
        localCandidate.address.indexOf(':') !== -1) {
      // Show IPv6 in []
      candidateElement.innerText =
          '[' + localCandidate.address + ']:' + localCandidate.port
          + ' <=> [' + remoteCandidate.address + ']:' + remoteCandidate.port;
    } else {
      candidateElement.innerText =
          localCandidate.address + ':' + localCandidate.port
          + ' <=> ' + remoteCandidate.address + ':' + remoteCandidate.port;
    }

    // Mark active local-candidate, remote candidate and candidate pair
    // bold in the table.
    // Disable getElementById restriction here, since |peerConnectionElement|
    // doesn't always have a valid selector ID.
    const statsContainer =
      // eslint-disable-next-line no-restricted-properties
        document.getElementById(peerConnectionElement.id + '-table-container');
    const activeConnectionClass = 'stats-table-active-connection';
    statsContainer.childNodes.forEach(node => {
      if (node.nodeName !== 'DETAILS' || !node.children[1]) {
        return;
      }
      const ids = [
        peerConnectionElement.id + '-table-' + activeCandidatePair.id,
        peerConnectionElement.id + '-table-' + localCandidate.id,
        peerConnectionElement.id + '-table-' + remoteCandidate.id,
      ];
      if (ids.includes(node.children[1].id)) {
        node.firstElementChild.classList.add(activeConnectionClass);
      } else {
        node.firstElementChild.classList.remove(activeConnectionClass);
      }
    });
    // Mark active candidate-pair graph bold.
    const statsGraphContainers = peerConnectionElement
      .getElementsByClassName('stats-graph-container');
    for (let i = 0; i < statsGraphContainers.length; i++) {
      const node = statsGraphContainers[i];
      if (node.nodeName !== 'DETAILS') {
        continue;
      }
      if (!node.id.startsWith(pcId + '-candidate-pair')) {
        continue;
      }
      if (node.id === pcId + '-candidate-pair-' + activeCandidatePair.id
          + '-graph-container') {
        node.firstElementChild.classList.add(activeConnectionClass);
      } else {
        node.firstElementChild.classList.remove(activeConnectionClass);
      }
    }
  } else {
    candidateElement.innerText = '(not connected)';
  }

  updateIceCandidateGrid(peerConnectionElement, r.statsById);
}

/**
 * Handles the report of stats originating from the legacy getStats() API.
 *
 * @param {!Object} data The object containing rid, lid, and reports, where
 *     reports is an array of stats reports. Each report contains id, type,
 *     and stats, where stats is the object containing timestamp and values,
 *     which is an array of strings, whose even index entry is the name of the
 *     stat, and the odd index entry is the value.
 */
function addLegacyStats(data) {
  if (currentGetStatsMethod != OPTION_GETSTATS_LEGACY) {
    return;  // Obsolete!
  }
  // Disable getElementById restriction here, since |getPeerConnectionId| does
  // not return valid selectors.
  const peerConnectionElement =
      // eslint-disable-next-line no-restricted-properties
      document.getElementById(getPeerConnectionId(data));
  if (!peerConnectionElement) {
    return;
  }

  for (let i = 0; i < data.reports.length; ++i) {
    const report = data.reports[i];
    statsTable.addStatsReport(peerConnectionElement, report);
    drawSingleReport(peerConnectionElement, report, true);
  }
}

/**
 * Notification that the audio debug recordings file selection dialog was
 * cancelled, i.e. recordings have not been enabled.
 */
function audioDebugRecordingsFileSelectionCancelled() {
  dumpCreator.clearAudioDebugRecordingsCheckbox();
}


/**
 * Notification that the event log recordings file selection dialog was
 * cancelled, i.e. recordings have not been enabled.
 */
function eventLogRecordingsFileSelectionCancelled() {
  dumpCreator.clearEventLogRecordingsCheckbox();
}
