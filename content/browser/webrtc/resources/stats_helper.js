// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!Object} stats A stats report.
 */
function generateLabel(key, stats) {
  let label = '';
  if (stats.hasOwnProperty(key)) {
    label += key + '=' + stats[key];
  }
  return label;
}

/**
 * Formats the display text used for a stats type that is shown
 * in the stats table or the stats graph.
 *
 * @param {!Object} rtcStats The RTCStats object.
 */
export function generateStatsLabel(rtcStats) {
  let label = rtcStats.type + ' (';
  let labels = [];
  if (['outbound-rtp', 'remote-outbound-rtp', 'inbound-rtp',
      'remote-inbound-rtp'].includes(rtcStats.type)) {
    labels = ['kind', 'mid', 'rid', 'ssrc', 'rtxSsrc', 'fecSsrc',
      'frameHeight', 'contentType',
      'active', 'scalabilityMode',
      'encoderImplementation', 'decoderImplementation',
      'powerEfficientEncoder', 'powerEfficientDecoder',
      '[codec]'];
  } else if (['local-candidate', 'remote-candidate'].includes(rtcStats.type)) {
    labels = ['candidateType', 'tcpType', 'relayProtocol'];
  } else if (rtcStats.type === 'codec') {
    labels = ['mimeType', 'payloadType'];
  } else if (['media-playout', 'media-source'].includes(rtcStats.type)) {
    labels = ['kind'];
  } else if (rtcStats.type === 'candidate-pair') {
    labels = ['state'];
  } else if (rtcStats.type === 'transport') {
    labels = ['iceState', 'dtlsState'];
  } else if (rtcStats.type === 'data-channel') {
    labels = ['label', 'state'];
  }
  labels = labels
    .map(stat => generateLabel(stat, rtcStats))
    .filter(label => !!label);
  if (labels.length) {
    label += labels.join(', ') + ', ';
  }
  label += 'id=' + rtcStats.id + ')';
  return label;
}
