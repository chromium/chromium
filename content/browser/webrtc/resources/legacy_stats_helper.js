// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains helpers related to the non-standard legacy stats.
// TODO(crbug.com/822696) remove legacy getStats() API.

import {$} from 'chrome://resources/js/util_ts.js';

// Specifies which stats should be drawn on the 'bweCompound' graph and how.
export const bweCompoundGraphConfig = {
    googAvailableSendBandwidth: {color: 'red'},
    googTargetEncBitrate: {color: 'purple'},
    googActualEncBitrate: {color: 'orange'},
    googRetransmitBitrate: {color: 'blue'},
    googTransmitBitrate: {color: 'green'},
  };

// Creates the legend section for the bweCompound graph.
// Returns the legend element.
export function createBweCompoundLegend(peerConnectionElement, reportId) {
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

// Specifies which legacy stats should be converted before drawn and how.
// |convertedName| is the name of the converted value, |convertFunction|
// is the function used to calculate the new converted value based on the
// original dataSeries.
export const legacyDataConversionConfig = {
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
};

export function isLegacyStatBlocklisted(type) {
  // Some legacy stats types that should not be added to the graph.
  return ['ssrc', 'googTrackId', 'googComponent', 'googLocalAddress',
    'googRemoteAddress', 'googFingerprint'].includes(type);
}

// Get report types for SSRC reports. Returns 'audio' or 'video' where this type
// can be deduced from existing stats labels. Otherwise empty string for
// non-SSRC reports or where type (audio/video) can't be deduced.
export function getLegacySsrcReportType(report) {
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
