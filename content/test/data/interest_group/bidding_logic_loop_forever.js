// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generateBid script that has an endless while loop. Used to test per buyer
// timeout, i.e., the bidder worklet for this script times out as expected.
function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                     trustedBiddingSignals, browserSignals) {
  while (1);
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {}
