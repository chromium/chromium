// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic generate bid script that offers a bid of 1 using the first ad's
// `renderURL` and, if present, the first adComponent's `renderURL`.
function generateBid(interestGroup, auctionSignals, perBuyerSignals,
  trustedBiddingSignals, browserSignals) {
  const ad = interestGroup.ads[0];

  // `auctionSignals` controls whether or not component auctions are allowed.
  let allowComponentAuction =
    typeof auctionSignals === 'string' &&
    auctionSignals.includes('bidderAllowsComponentAuction');

  let result = {
    'ad': ad, 'bid': 1, 'render': ad.renderURL,
    'allowComponentAuction': allowComponentAuction
  };
  if (interestGroup.adComponents && interestGroup.adComponents[0])
    result.adComponents = [interestGroup.adComponents[0].renderURL];
  return result;
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
  browserSignals) {
  registerAdBeacon({
    'click': browserSignals.interestGroupOwner + "/report_event.html",
    'reserved.top_navigation_commit':
      browserSignals.interestGroupOwner + "/report_event.html"
  });
}
