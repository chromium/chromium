// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  forDebuggingOnly.reportAdAuctionLoss(
    interestGroup.owner + '/echo?bidder_debug_report_loss/' +
    interestGroup.name + '/before_error');

  throw 'Here\'s an error';

  forDebuggingOnly.reportAdAuctionLoss(
    interestGroup.owner + '/echo?bidder_debug_report_loss/' +
    interestGroup.name + '/after_error');
}
