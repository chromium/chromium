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
  return {
      'ad': ad,
      'bid': 2,
      'render': ad.renderUrl,
      'adComponents': [interestGroup.adComponents[0].renderUrl]
  };
}

function validateInterestGroup(interestGroup) {
  if (!interestGroup)
    throw 'No interest group';

  if (Object.keys(interestGroup).length !== 9) {
    throw 'Wrong number of interestGroupFields ' +
        JSON.stringify(interestGroup);
  }

  if (interestGroup.name !== 'cars')
    throw 'Wrong interestGroup.name ' + interestGroup.name;
  if (!interestGroup.owner.startsWith('https://a.test'))
    throw 'Missing a.test in owner ' + interestGroup.owner;

  if (!interestGroup.biddingLogicUrl.startsWith('https://a.test') ||
      !interestGroup.biddingLogicUrl.endsWith(
          '/bidding_argument_validator.js')) {
    throw 'Incorrect biddingLogicUrl ' + interestGroup.biddingLogicUrl;
  }

  if (!interestGroup.dailyUpdateUrl.startsWith('https://a.test') ||
      !interestGroup.dailyUpdateUrl.endsWith(
          '/not_found_daily_update_url.json')) {
    throw 'Incorrect dailyUpdateUrl ' + interestGroup.dailyUpdateUrl;
  }

  if (!interestGroup.trustedBiddingSignalsUrl.startsWith('https://a.test') ||
      !interestGroup.trustedBiddingSignalsUrl.includes(
          'trusted_bidding_signals.json')) {
    throw 'Incorrect trustedBiddingSignalsUrl ' +
        interestGroup.trustedBiddingSignalsUrl;
  }

  trustedBiddingSignalsKeysJson =
      JSON.stringify(interestGroup.trustedBiddingSignalsKeys);
  if (trustedBiddingSignalsKeysJson !== "[\"key1\"]") {
    throw 'Incorrect trustedBiddingSignalsKeys ' +
        trustedBiddingSignalsKeysJson;
  }

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
    throw 'Wrong ads.length ' + interestGroup.ads.length;
  if (interestGroup.ads[0].renderUrl !== 'https://example.com/render')
    throw 'Wrong ads[0].renderUrl ' + interestGroup.ads[0].renderUrl;
  const adMetadataJSON = JSON.stringify(interestGroup.ads[0].metadata);
  if (adMetadataJSON !== '{"ad":"metadata","here":[1,2,3]}')
    throw 'Wrong ad[0].metadata ' + adMetadataJSON;

  if (interestGroup.adComponents.length !== 1)
    throw 'Wrong adComponents.length ' + interestGroup.adComponents.length;
  if (interestGroup.adComponents[0].renderUrl !==
        'https://example.com/render-component') {
    throw 'Wrong adComponents[0].renderUrl ' +
        interestGroup.adComponents[0].renderUrl;
  }
  if (interestGroup.adComponents[0].metadata !== undefined) {
    throw 'interestGroup.adComponents[0].metadata ' +
        adMetadataJinterestGroup.adComponents[0].metadataSON;
  }
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
  if (Object.keys(browserSignals).length !== 5) {
    throw 'Wrong number of browser signals fields ' +
        JSON.stringify(browserSignals);
  }
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.seller.startsWith('https://b.test'))
    throw 'Wrong seller ' + browserSignals.seller;
  if ('topLevelSeller' in browserSignals)
    throw 'Wrong topLevelSeller ' + browserSignals.topLevelSeller;
  if (browserSignals.joinCount !== 1)
    throw 'Wrong joinCount ' + browserSignals.joinCount;
  if (browserSignals.bidCount !== 0)
    throw 'Wrong bidCount ' + bidCount;
  if (browserSignals.prevWins.length !== 0)
    throw 'Wrong prevWins ' + JSON.stringify(browserSignals.prevWins);
}
