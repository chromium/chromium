// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

function reportWin(
  auctionSignals, perBuyerSignals, sellerSignals,
  browserSignals) {

  sendReportTo(browserSignals.interestGroupOwner + '/echoall?report_bidder');
}
