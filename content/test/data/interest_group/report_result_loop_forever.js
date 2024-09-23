// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return {desirability: bid, allowComponentAuction: true};
}

function reportResult(auctionConfig, browserSignals) {
  while (1);
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': auctionConfig.seller + '/report_seller',
  };
}
