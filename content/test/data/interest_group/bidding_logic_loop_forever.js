// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generateBid script that has an endless while loop. Used to test per buyer
// timeout, i.e., the bidder worklet for this script times out as expected.
// It's also used to test that loss report URLs of reportAdAuctionLoss() called
// before the script timed out is sent.
function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                     trustedBiddingSignals, browserSignals) {
  forDebuggingOnly.reportAdAuctionLoss(
    interestGroup.owner + '/echo?bidder_debug_report_loss/' +
    interestGroup.name + '/before_timeout');

  while (1);

  forDebuggingOnly.reportAdAuctionLoss(
    interestGroup.owner + '/echo?bidder_debug_report_loss/' +
    interestGroup.name + '/after_timeout');
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {}
