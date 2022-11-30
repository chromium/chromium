// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  if (browserSignals.topWindowHostname != 'a.test')
    throw new Error('Unexpected hostname:'  + browserSignals.topWindowHostname);
  return bid;
}

function reportResult(
    auctionConfig, browserSignals) {
  if (browserSignals.topWindowHostname != 'a.test')
    throw new Error('Unexpected hostname:'  + browserSignals.topWindowHostname);
  sendReportTo(auctionConfig.seller + '/echoall?report_seller');
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': auctionConfig.seller + '/report_seller',
  };
}
