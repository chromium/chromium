// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

import {createIceCandidateGrid, updateIceCandidateGrid} from './candidate_grid.js';
import {MAX_STATS_DATA_POINT_BUFFER_SIZE} from './data_series.js';
import {
    DumpCreator,
    peerConnectionDataStore,
    userMediaRequests,
    addRtcStatsEvent
} from './dump_creator.js';
import {PeerConnectionUpdateTable} from './peer_connection_update_table.js';
import {drawSingleRtcStats, removeStatsReportGraphs} from './stats_graph_helper.js';
import {StatsRatesCalculator} from './stats_rates_calculator.js';
import {StatsTable} from './stats_table.js';
import {TabView} from './tab_view.js';
import {UserMediaTable} from './user_media_table.js';

let tabView = null;
let peerConnectionUpdateTable = null;
let statsTable = null;
let userMediaTable = null;
let dumpCreator = null;

const searchParameters = new URLSearchParams(window.location.search);

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
   * @param {!Object} update The object contains keys "timestamp", "type", and
   *   "value".
   */
  addUpdate(update) {
    const time = new Date(parseFloat(update.time));
    this.record_.updateLog.push({
      type: update.type,
      value: update.value,
      timestamp: update.timestamp,
    });
  }
}

function initialize() {
  dumpCreator = new DumpCreator($('content-root'));

  tabView = new TabView($('content-root'));
  peerConnectionUpdateTable = new PeerConnectionUpdateTable();
  statsTable = new StatsTable();
  userMediaTable = new UserMediaTable(tabView, userMediaRequests);

  // Add listeners for all the updates that get sent from webrtc_internals.cc.
  addWebUiListener('add-peer-connection', addPeerConnection);
  addWebUiListener('update-peer-connection', updatePeerConnection);
  addWebUiListener('update-all-peer-connections', updateAllPeerConnections);
  addWebUiListener('remove-peer-connection', removePeerConnection);
  addWebUiListener('add-standard-stats', addStandardStats);
  addWebUiListener('add-media', (data) => {
    userMediaRequests.push(data);
    userMediaTable.addMedia(data)
    const constraints = {};
    ['audio', 'video'].forEach(kind => {
      if (data[kind] !== undefined) {
        if (data[kind] === '') {
          constraints[kind] = true;
        } else {
          constraints[kind] = data[kind];
        }
      }
    });
    addRtcStatsEvent(
      'navigator.mediaDevices.' + data.request_type,
      [data.rid, 0].join('-'),
      constraints,
      // correlation id.
      [data.request_type, data.rid, data.pid, data.request_id].join('-'),
      data.url,
      data.timestamp
    );
  });
  addWebUiListener('update-media', (data) => {
    userMediaRequests.push(data);
    userMediaTable.updateMedia(data);
    if (data.error) {
      addRtcStatsEvent(
        'navigator.mediaDevices.' + data.request_type + 'OnFailure',
        [data.rid, 0].join('-'),
        {
          error: data.error,
          error_message: data.error_message
        },
        // correlation id.
        [data.request_type, data.rid, data.pid, data.request_id].join('-'),
        data.timestamp
      );
    } else {
      const tracks = [];
      if (data.audio_track_info !== 'null') {
        const track_data = JSON.parse(data.audio_track_info);
        tracks.push(['audio', track_data.id, track_data.label, data.stream_id]);
      }
      if (data.video_track_info !== 'null') {
        const track_data = JSON.parse(data.video_track_info);
        tracks.push(['video', track_data.id, track_data.label, data.stream_id]);
      }
      addRtcStatsEvent(
        'navigator.mediaDevices.' + data.request_type + 'OnSuccess',
        [data.rid, 0].join('-'),
        tracks,
        // correlation id.
        [data.request_type, data.rid, data.pid, data.request_id].join('-'),
        data.timestamp
      );
    }
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
  addWebUiListener(
      'data-channel-recordings-file-selection-cancelled',
      dataChannelRecordingsFileSelectionCancelled);

  // Request initial startup parameters.
  sendWithPromise('finishedDOMLoad').then(params => {
    if (params.audioDebugRecordingsEnabled) {
      dumpCreator.setAudioDebugRecordingsCheckbox();
    }
    if (params.eventLogRecordingsEnabled) {
      dumpCreator.setEventLogRecordingsCheckbox();
    }
    if (params.dataChannelRecordingsEnabled) {
      dumpCreator.setDataChannelRecordingsCheckbox();
    }
    dumpCreator.setEventLogRecordingsCheckboxMutability(
        params.eventLogRecordingsToggleable);
  });

  // Requests stats from all peer connections every second unless specified via
  // ?statsInterval=(milliseconds >= 100ms)
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

  addRtcStatsEvent(
    'create',
    null,
    {
      hardwareConcurrency: navigator.hardwareConcurrency,
      userAgentData: navigator.userAgentData,
      deviceMemory: navigator.deviceMemory,
      screen: {
        width: window.screen.availWidth,
        height: window.screen.availHeight,
        devicePixelRatio: window.devicePixelRatio,
      },
      window: {
        width: window.innerWidth,
        height: window.innerHeight,
      },
    },
    Date.now()
  );
}
document.addEventListener('DOMContentLoaded', initialize);

/**
 * Sends a request to the browser to get peer connection statistics from the
 * standard getStats() API (promise-based).
 */
function requestStats() {
  if (Object.keys(peerConnectionDataStore).length > 0) {
    chrome.send('getStandardStats');
  }
}

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
  peerConnectionDataStore[peerConnectionElement.id].addUpdate(update);
  let value = undefined;
  if (update.value.length) {
    if (update.value[0] === '{') {
      value = JSON.parse(update.value);
    } else {
      value = update.value;
    }
  }
  addRtcStatsEvent(
    update.type,
    getPeerConnectionId(update),
    value,
    update.timestamp
  );
}


/** Browser message handlers. */


/**
 * Removes all information about a peer connection.
 * Use ?keepRemovedConnections url parameter to prevent the removal.
 *
 * @param {!Object<number>} data The object containing the rid and lid of a peer
 *     connection.
 */
function removePeerConnection(data) {
  // Disable getElementById restriction here, since |getPeerConnectionId| does
  // not return valid selectors.
  // eslint-disable-next-line no-restricted-properties

  const element = document.getElementById(getPeerConnectionId(data));
  if (element && !searchParameters.has('keepRemovedConnections')) {
    removeStatsReportGraphs(element);
    delete peerConnectionDataStore[element.id];
    tabView.removeTab(element.id);
  }
  // The rtcstats variant could remove from the array based on
  // peerconnection id but only logs the peerconnection went away.
  addRtcStatsEvent(
    'remove',
    getPeerConnectionId(data),
    undefined,
    Date.now()
  );
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

  if (data.rtcConfiguration) {
    addRtcStatsEvent(
      'create',
      getPeerConnectionId(data),
      JSON.parse(data.rtcConfiguration),
      data.url,
      data.timestamp
    );
  }
  // Nonstandard legacy constraints.
  if (data.constraints) {
    addRtcStatsEvent(
      'constraints',
      getPeerConnectionId(data),
      JSON.parse(data.constraints),
      data.url,
      data.timestamp
    );
  }

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
  // Create a map from the stats entries so it behaves like a getStats maplike
  // and then sort it.
  const rtcReport = sortStatsReport(new Map(data.reports));
  addRtcStatsEvent(
    'getStats',
    getPeerConnectionId(data),
    rtcReport.entries().reduce((o, [k, v]) => {
      o[k] = v;
      return o;
    }, {}),
    data.timestamp
  );

  // This augments stats with [delta] values.
  statsRatesCalculator.addStatsReport(rtcReport);
  rtcReport.forEach(rtcStats => {
    statsTable.addRtcStats(peerConnectionElement, rtcStats);
    drawSingleRtcStats(peerConnectionElement, rtcStats);
  });

  let ids = [];
  rtcReport.forEach(report => {
    if (!(report.type === 'transport' && report.selectedCandidatePairId)) {
      return;
    }
    const activeCandidatePair = rtcReport.get(report.selectedCandidatePairId);
    const remoteCandidate =
        rtcReport.get(activeCandidatePair.remoteCandidateId);
    const localCandidate = rtcReport.get(activeCandidatePair.localCandidateId);

    const candidateElement = peerConnectionElement
      .getElementsByClassName('candidatepair')[0].firstElementChild;
    candidateElement.innerText = '';
    if (!(localCandidate && remoteCandidate)) {
      return;
      candidateElement.innerText = '(not connected)';
    }

    if (localCandidate.address &&
        localCandidate.address.indexOf(':') !== -1) {
      // Show IPv6 in []
      candidateElement.innerText +='[' + localCandidate.address + ']';
    } else {
      candidateElement.innerText += localCandidate.address || '(not set)';
    }
    candidateElement.innerText += ':' + localCandidate.port + ' <=> ';

    if (remoteCandidate.address &&
        remoteCandidate.address.indexOf(':') !== -1) {
      // Show IPv6 in []
      candidateElement.innerText +='[' + remoteCandidate.address + ']';
    } else {
      candidateElement.innerText += remoteCandidate.address || '(not set)';
    }
    candidateElement.innerText += ':' + remoteCandidate.port;
    ids = ids.concat([
      peerConnectionElement.id + '-table-' + activeCandidatePair.id,
      peerConnectionElement.id + '-table-' + localCandidate.id,
      peerConnectionElement.id + '-table-' + remoteCandidate.id,
    ]);
  });
  // Mark active local-candidate, remote candidate and candidate pair
  // bold in the table.
  // Disable getElementById restriction here, since |peerConnectionElement|
  // doesn't always have a valid selector ID.
  // First remove bold from each, then re-add for each active pair..
  const statsContainer =
    // eslint-disable-next-line no-restricted-properties
      document.getElementById(peerConnectionElement.id + '-table-container');
  const activeConnectionClass = 'stats-table-active-connection';
  statsContainer.childNodes.forEach(node => {
    if (node.nodeName !== 'DETAILS' || !node.children[1]) {
      return;
    }
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
    if (ids.includes(node.children[1].id)) {
      node.firstElementChild.classList.add(activeConnectionClass);
    } else {
      node.firstElementChild.classList.remove(activeConnectionClass);
    }
  }

  updateIceCandidateGrid(peerConnectionElement, rtcReport);

  // Mark inactive outbound-rtp in grey.
  const inactiveStatsIds = [];
  const inactiveRtpStatsClass = 'stats-table-rtp-inactive';
  rtcReport.forEach(rtcStats => {
    if (!(rtcStats.type === 'outbound-rtp')) {
      return;
    }
    if (rtcStats.active === false) {
      inactiveStatsIds.push(
          peerConnectionElement.id + '-details-' + rtcStats.id);
    }
  });
  statsContainer.childNodes.forEach(node => {
    if (node.nodeName !== 'DETAILS') {
      return;
    }
    if (inactiveStatsIds.includes(node.id)) {
      node.classList.add(inactiveRtpStatsClass);
    } else {
      node.classList.remove(inactiveRtpStatsClass);
    }
  });
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


function dataChannelRecordingsFileSelectionCancelled() {
  dumpCreator.clearDataChannelRecordingsCheckbox();
}

// Returns a sorted version of the stats report as a Map.
// 1. outbound-rtps, sorted by encodingIndex
// 2. inbound-rtps
// 3. everything else
function sortStatsReport(report) {
  const getOutboundRtpsForMid = (report, mid) => {
    const outboundRtpsByEncodingIndex = new Map();
    for (const stats of report.values()) {
      if (stats.type !== 'outbound-rtp' || stats.mid !== mid) {
        continue;
      }
      let encodingIndex = stats.encodingIndex ? Number(stats.encodingIndex) : 0;
      outboundRtpsByEncodingIndex.set(encodingIndex, stats);
    }
    const orderedOutboundRtps = [];
    for (let i = 0; i < outboundRtpsByEncodingIndex.size; ++i) {
      orderedOutboundRtps.push(outboundRtpsByEncodingIndex.get(i));
    }
    return orderedOutboundRtps;
  }
  // Categorize into outbound-rtp, inbound-rtp and other categories.
  let outboundRtps = [];
  let inboundRtps = [];
  let otherStats = [];
  const midsIncluded = new Set();
  for (const stats of report.values()) {
    if (stats.type === 'outbound-rtp') {
      if (stats.mid !== undefined) {
        if (midsIncluded.has(stats.mid)) {
          continue;  // This outbound-rtp has already been included.
        }
        midsIncluded.add(stats.mid);
        // Add all outbound-rtps for this mid in encodingIndex order.
        outboundRtps = outboundRtps.concat(
            getOutboundRtpsForMid(report, stats.mid));
      } else {
        // It's unexpected that an outbound-rtp does not have a mid due to
        // outbound-rtps being created after O/A, but just in case...
        outboundRtps.push(stats);
      }
    } else if (stats.type === 'inbound-rtp') {
      inboundRtps.push(stats);
    } else {
      otherStats.push(stats);
    }
  }
  // Re-build the internal reports in our new preferred order.
  const sortedReport = new Map();
  for (const outboundRtp of outboundRtps) {
    sortedReport.set(outboundRtp.id, outboundRtp);
  }
  for (const inboundRtp of inboundRtps) {
    sortedReport.set(inboundRtp.id, inboundRtp);
  }
  for (const other of otherStats) {
    sortedReport.set(other.id, other);
  }
  return sortedReport;
}
