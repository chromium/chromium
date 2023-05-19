// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals, directFromSellerSignals) {
  validateDirectFromSellerSignals(directFromSellerSignals);

  const ad = interestGroup.ads[0];

  // `auctionSignals` controls whether or not component auctions are allowed.
  let allowComponentAuction =
      typeof auctionSignals === 'string' &&
      auctionSignals.includes('bidderAllowsComponentAuction');

  // Bid 2 to outbid other parties bidding 1 at auction.
  let result = {
    'ad': ad,
    'bid': 2,
    'render': ad.renderURL,
    'allowComponentAuction': allowComponentAuction
  };
  if (interestGroup.adComponents && interestGroup.adComponents[0])
    result.adComponents = [interestGroup.adComponents[0].renderURL];
  return result;

}

function validateDirectFromSellerSignals(directFromSellerSignals) {
  const perBuyerSignalsJSON =
      JSON.stringify(directFromSellerSignals.perBuyerSignals);
  if (perBuyerSignalsJSON !== 'null') {
    throw 'Wrong directFromSellerSignals.perBuyerSignals ' +
        perBuyerSignalsJSON;
  }
  const auctionSignalsJSON =
      JSON.stringify(directFromSellerSignals.auctionSignals);
  if (auctionSignalsJSON !== 'null') {
    throw 'Wrong directFromSellerSignals.auctionSignals ' +
        auctionSignalsJSON;
  }
}
