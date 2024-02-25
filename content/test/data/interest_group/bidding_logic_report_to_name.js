// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  const ad = interestGroup.ads[0];
  forDebuggingOnly.reportAdAuctionWin(interestGroup.name + '_debug_win_report');
  return {'ad': ad, 'bid': 1, 'render': ad.renderURL};
}

function reportWin(
  auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {

  // Smuggle the report-to destination in the InterestGroup name, so caller can
  // control the destination.
  sendReportTo(browserSignals.interestGroupName);
}
