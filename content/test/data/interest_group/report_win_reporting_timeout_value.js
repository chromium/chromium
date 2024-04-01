// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic generate bid script that offers a bid of 1 using the first ad's
// `renderURL` and, if present, the first adComponent's `renderURL`.
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  const ad = interestGroup.ads[0];

  return {
    'ad': ad,
    'bid': 1,
    'render': ad.renderURL,
    'allowComponentAuction': true
  };
}

function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo(
      browserSignals.interestGroupOwner +
      '/echoall?report_bidder,reportingTimeout=' +
      browserSignals.reportingTimeout);
}
