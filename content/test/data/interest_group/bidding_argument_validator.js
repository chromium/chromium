// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  validateInterestGroup(interestGroup);
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateTrustedBiddingSignals(trustedBiddingSignals);
  validateBrowserSignals(browserSignals);

  // Bid 2 to outbid other parties bidding 1 at auction.
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 2, 'render': ad.renderUrl};
}

function validateInterestGroup(interestGroup) {
  if (!interestGroup)
    throw 'No interest group';
  if (interestGroup.name !== 'cars')
    throw 'Wrong interestGroup.name ' + interestGroup.name;
  if (!interestGroup.owner.includes('a.test'))
    throw 'Missing a.test in owner ' + interestGroup.owner;
  // TODO(crbug.com/1186444): Consider validating URL fields like
  // interestGroup.biddingLogicUrl once we decide what to do about URL
  // normalization.

  // If userBiddingSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail. The order of properties produced by stringify() isn't guaranteed by
  // the ECMAScript standard, but some sites depend on the V8 behavior of
  // serializing in declaration order.
  const userBiddingSignalsJSON =
      JSON.stringify(interestGroup.userBiddingSignals);
  if (userBiddingSignalsJSON !== '{"some":"json","data":{"here":[1,2,3]}}')
    throw 'Wrong userBiddingSignals ' + userBiddingSignalsJSON;
  if (interestGroup.ads.length !== 1)
    throw 'Wrong ads.length ' + ads.length;
  const adMetadataJSON = JSON.stringify(interestGroup.ads[0].metadata);
  if (adMetadataJSON !== '{"ad":"metadata","here":[1,2,3]}')
    throw 'Wrong ad[0].metadata ' + adMetadataJSON;
}

function validateAuctionSignals(auctionSignals) {
  const auctionSignalsJSON = JSON.stringify(auctionSignals);
  if (auctionSignalsJSON !== '{"so":"I","hear":["you","like","json"]}')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
}

function validatePerBuyerSignals(perBuyerSignals) {
  const perBuyerSignalsJson = JSON.stringify(perBuyerSignals);
  if (perBuyerSignalsJson !== '{"signalsForBuyer":1}')
    throw 'Wrong perBuyerSignas ' + perBuyerSignalsJson;
}

function validateTrustedBiddingSignals(trustedBiddingSignals) {
  const trustedBiddingSignalsJSON = JSON.stringify(trustedBiddingSignals);
  if (trustedBiddingSignalsJSON !== '{"key1":"1"}')
    throw 'Wrong trustedBiddingSignals ' + trustedBiddingSignalsJSON;
}

function validateBrowserSignals(browserSignals) {
  if (Object.keys(browserSignals).length !== 5)
    throw 'Wrong number of browser signals fields ' +
        JSON.stringify(browserSignals);
  // TODO(crbug.com/1186444): Test in subframe too.
  if (!browserSignals.topWindowHostname.includes('a.test'))
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.seller.includes('a.test'))
    throw 'Wrong seller ' + browserSignals.seller;
  if (browserSignals.joinCount !== 1)
    throw 'Wrong joinCount ' + browserSignals.joinCount;
  if (browserSignals.bidCount !== 0)
    throw 'Wrong bidCount ' + bidCount;
  if (browserSignals.prevWins.length !== 0)
    throw 'Wrong prevWins ' + JSON.stringify(browserSignals.prevWins);
}
