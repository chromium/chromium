// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  // `auctionSignals` controls whether or not component auctions are allowed.
  let allowComponentAuction =
      typeof auctionConfig.auctionSignals === 'string' &&
      auctionConfig.auctionSignals.includes('sellerAllowsComponentAuction');
  return {desirability: bid,
          allowComponentAuction:allowComponentAuction};
}

function reportResult(auctionConfig, browserSignals) {
  sendReportTo(auctionConfig.seller + '/echoall?report_seller');
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': auctionConfig.seller + '/report_seller',
  };
}
