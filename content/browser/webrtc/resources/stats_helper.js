// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!Object} statsValues The object containing stats, an
 *     array [key1, val1, key2, val2, ...] so searching a certain
 *     key needs to ensure it does not collide with a value.
 */
function generateLabel(key, statsValues) {
  let label = '';
  const statIndex = statsValues.findIndex((value, index) => {
    return value === key && index % 2 === 0;
  });
  if (statIndex !== -1) {
    label += key + '=' + statsValues[statIndex + 1];
  }
  return label;
}

/**
 * Formats the display text used for a stats type that is shown
 * in the stats table or the stats graph.
 *
 * @param {!Object} report The object containing stats, which is the object
 *     containing timestamp and values, which is an array of strings, whose
 *     even index entry is the name of the stat, and the odd index entry is
 *     the value.
 */
export function generateStatsLabel(report) {
  let label = report.type + ' (';
  let labels = [];
  if (['outbound-rtp', 'remote-outbound-rtp', 'inbound-rtp',
      'remote-inbound-rtp'].includes(report.type) && report.stats.values) {
    labels = ['kind', 'mid', 'rid', 'ssrc', 'rtxSsrc', 'fecSsrc',
      'scalabilityMode',
      'encoderImplementation', 'decoderImplementation',
      'powerEfficientEncoder', 'powerEfficientDecoder',
      '[codec]'];
  } else if (['local-candidate', 'remote-candidate'].includes(report.type)) {
    labels = ['candidateType', 'tcpType', 'relayProtocol'];
  } else if (report.type === 'codec') {
    labels = ['mimeType', 'payloadType'];
  } else if (['media-playout', 'media-source'].includes(report.type)) {
    labels = ['kind'];
  } else if (report.type === 'candidate-pair') {
    labels = ['state'];
  } else if (report.type === 'transport') {
    labels = ['iceState', 'dtlsState'];
  }
  labels = labels
    .map(stat => generateLabel(stat, report.stats.values))
    .filter(label => !!label);
  if (labels.length) {
    label += labels.join(', ') + ', ';
  }
  label += 'id=' + report.id + ')';
  return label;
}
