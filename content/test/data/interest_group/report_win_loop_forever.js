// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                     trustedBiddingSignals, browserSignals) {
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderURL,
          'allowComponentAuction': true};
}

// reportWin script that has an endless while loop. Used to test reporting
// timeout working as expected for reportWin.
function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {
  while (1);
}
