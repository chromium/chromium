// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals, directFromSellerSignals) {
  validateInterestGroup(interestGroup);
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateTrustedBiddingSignals(trustedBiddingSignals);
  validateBrowserSignals(browserSignals, /*isGenerateBid=*/true);
  validateDirectFromSellerSignals(directFromSellerSignals);

  // Bid 2 to outbid other parties bidding 1 at auction.
  const ad = interestGroup.ads[0];
  return {
      'ad': ad,
      'bid': 2,
      'bidCurrency': 'USD',
      'adCost': 3,
      'render': ad.renderURL,
      'adComponents': [interestGroup.adComponents[0].renderURL],
      'allowComponentAuction': true,
  };
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals, directFromSellerSignals) {
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateSellerSignals(sellerSignals);
  validateBrowserSignals(browserSignals, /*isGenerateBid=*/false);
  validateDirectFromSellerSignals(directFromSellerSignals);

  sendReportTo(browserSignals.interestGroupOwner + '/echo?report_bidder');
}

function validateInterestGroup(interestGroup) {
  if (!interestGroup)
    throw 'No interest group';

  if (Object.keys(interestGroup).length !== 18) {
    throw 'Wrong number of interestGroupFields ' +
        JSON.stringify(interestGroup);
  }

  if (interestGroup.name !== 'cars')
    throw 'Wrong interestGroup.name ' + interestGroup.name;
  if (!interestGroup.owner.startsWith('https://a.test'))
    throw 'Missing a.test in owner ' + interestGroup.owner;

  // Note that this field is deprecated.
  if (interestGroup.useBiddingSignalsPrioritization !== false) {
    throw 'Incorrect useBiddingSignalsPrioritization ' +
        interestGroup.useBiddingSignalsPrioritization;
  }

  if (interestGroup.enableBiddingSignalsPrioritization !== false) {
    throw 'Incorrect enableBiddingSignalsPrioritization ' +
        interestGroup.enableBiddingSignalsPrioritization;
  }

  if (Object.keys(interestGroup.priorityVector).length !== 1 ||
    interestGroup.priorityVector['FOO'] !== 2) {
    throw 'Incorrect priorityVector ' +
        JSON.stringify(interestGroup.priorityVector);
  }

  if (!interestGroup.biddingLogicURL.startsWith('https://a.test') ||
      !interestGroup.biddingLogicURL.endsWith(
          '/component_auction_bidding_argument_validator.js')) {
    throw 'Incorrect biddingLogicURL ' + interestGroup.biddingLogicURL;
  }

  if (!interestGroup.biddingLogicUrl.startsWith('https://a.test') ||
      !interestGroup.biddingLogicUrl.endsWith(
          '/component_auction_bidding_argument_validator.js')) {
    throw 'Incorrect biddingLogicUrl ' + interestGroup.biddingLogicUrl;
  }

  if (!interestGroup.updateURL.startsWith('https://a.test') ||
      !interestGroup.updateURL.endsWith('/not_found_update_url.json')) {
    throw 'Incorrect updateURL ' + interestGroup.updateURL;
  }

  if (interestGroup.updateUrl !== interestGroup.updateURL) {
    throw 'Incorrect updateUrl ' + interestGroup.updateUrl;
  }

  // TODO(crbug.com/40258629): Remove this block and decrease number of
  // expected keys above when removing support for dailyUpdateUrl.
  if (!interestGroup.dailyUpdateUrl.startsWith('https://a.test') ||
      !interestGroup.dailyUpdateUrl.endsWith('/not_found_update_url.json')) {
    throw 'Incorrect dailyUpdateUrl ' + interestGroup.dailyUpdateUrl;
  }

  if (interestGroup.executionMode !== 'compatibility')
    throw 'Incorrect executionMode ' + interestGroup.executionMode;

  if (!interestGroup.trustedBiddingSignalsURL.startsWith('https://a.test') ||
      !interestGroup.trustedBiddingSignalsURL.includes(
          'trusted_bidding_signals.json')) {
    throw 'Incorrect trustedBiddingSignalsURL ' +
        interestGroup.trustedBiddingSignalsURL;
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

  if (interestGroup.trustedBiddingSignalsSlotSizeMode != 'none') {
    throw 'Incorrect trustedBiddingSignalsSlotSizeMode ' +
        interestGroup.trustedBiddingSignalsSlotSizeMode;
  }

  // TODO(crbug.com/40172488): Consider validating URL fields like
  // interestGroup.biddingLogicURL once we decide what to do about URL
  // normalization.

  // If userBiddingSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail. The order of properties produced by stringify() isn't guaranteed by
  // the ECMAScript standard, but some sites depend on the V8 behavior of
  // serializing in declaration order.
  const userBiddingSignalsJSON =
      JSON.stringify(interestGroup.userBiddingSignals);
  if (userBiddingSignalsJSON !== '{"some":"json","stuff":{"here":[1,2]}}')
    throw 'Wrong userBiddingSignals ' + userBiddingSignalsJSON;

  if (interestGroup.ads.length !== 1)
    throw 'Wrong ads.length ' + interestGroup.ads.length;
  if (interestGroup.ads[0].renderURL !== 'https://example.com/render')
    throw 'Wrong ads[0].renderURL ' + interestGroup.ads[0].renderURL;
  if (interestGroup.ads[0].renderUrl !== 'https://example.com/render')
    throw 'Wrong ads[0].renderUrl ' + interestGroup.ads[0].renderUrl;
  const adMetadataJson = JSON.stringify(interestGroup.ads[0].metadata);
  if (adMetadataJson !== '{"ad":"metadata","here":[1,2,3]}')
    throw 'Wrong ad[0].metadata ' + adMetadataJson;

  if (interestGroup.adComponents.length !== 1)
    throw 'Wrong adComponents.length ' + interestGroup.adComponents.length;
  if (interestGroup.adComponents[0].renderURL !==
        'https://example.com/render-component') {
    throw 'Wrong adComponents[0].renderURL ' +
        interestGroup.adComponents[0].renderURL;
  }
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
  const auctionSignalsJson = JSON.stringify(auctionSignals);
  if (auctionSignalsJson !== '["component auction signals"]')
    throw 'Wrong auctionSignals ' + auctionSignalsJson;
}

function validatePerBuyerSignals(perBuyerSignals) {
  const perBuyerSignalsJson = JSON.stringify(perBuyerSignals);
  if (perBuyerSignalsJson !== '["component buyer signals"]')
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
}

function validateTrustedBiddingSignals(trustedBiddingSignals) {
  const trustedBiddingSignalsJson = JSON.stringify(trustedBiddingSignals);
  if (trustedBiddingSignalsJson !== '{"key1":"1"}')
    throw 'Wrong trustedBiddingSignals ' + trustedBiddingSignalsJson;
}

function validateBrowserSignals(browserSignals, isGenerateBid) {
  // Common fields for generateBid() and reportWin().
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.seller.startsWith('https://d.test'))
    throw 'Wrong seller ' + browserSignals.seller;
  if (!browserSignals.topLevelSeller.startsWith('https://b.test'))
    throw 'Wrong topLevelSeller ' + browserSignals.topLevelSeller;

  if (isGenerateBid) {
    if (Object.keys(browserSignals).length !== 11) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    if (browserSignals.joinCount !== 1)
      throw 'Wrong joinCount ' + browserSignals.joinCount;
    if (browserSignals.bidCount !== 0)
      throw 'Wrong bidCount ' + browserSignals.bidCount;
    if (browserSignals.prevWins.length !== 0)
      throw 'Wrong prevWins ' + JSON.stringify(browserSignals.prevWins);
    if (browserSignals.prevWinsMs.length !== 0)
      throw 'Wrong prevWinsMs ' + JSON.stringify(browserSignals.prevWinsMs);
    if (browserSignals.adComponentsLimit !== 40)
      throw 'Wrong adComponentsLimit ' + browserSignals.adComponentsLimit;
    if (browserSignals.forDebuggingOnlyInCooldownOrLockout)
      throw 'Wrong forDebuggingOnlyInCooldownOrLockout ' +
          browserSignals.forDebuggingOnlyInCooldownOrLockout;
    if (browserSignals.multiBidLimit !== 1)
      throw 'Wrong multiBidLimit ' + browserSignals.multiBidLimit;
  } else {
    // FledgePassKAnonStatusToReportWin feature adds a new parameter
    // KAnonStatus to reportWin(), which is under a Finch trial for some enabled
    // tests.
    // TODO(xtlsheep): Check length only equals to 17 after
    // FledgePassKAnonStatusToReportWin is completely turned on.
    if (Object.keys(browserSignals).length !== 16 &&
        Object.keys(browserSignals).length !== 17) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    if (!browserSignals.interestGroupOwner.startsWith('https://a.test'))
      throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
    if (browserSignals.interestGroupName !== 'cars')
      throw 'Wrong interestGroupName ' + browserSignals.interestGroupName;
    if (browserSignals.renderURL !== "https://example.com/render")
      throw 'Wrong renderURL ' + browserSignals.renderURL;
    if (browserSignals.renderUrl !== "https://example.com/render")
      throw 'Wrong renderUrl ' + browserSignals.renderUrl;
    if (browserSignals.bid !== 2)
      throw 'Wrong bid ' + browserSignals.bid;
    if (browserSignals.bidCurrency !== 'USD')
      throw 'Wrong bidCurrency ' + browserSignals.bidCurrency;
    if (browserSignals.highestScoringOtherBid !== 0) {
      throw 'Wrong highestScoringOtherBid ' +
          browserSignals.highestScoringOtherBid;
    }
    if (browserSignals.highestScoringOtherBidCurrency !== 'CAD') {
      throw 'Wrong highestScoringOtherBidCurrency ' +
          browserSignals.highestScoringOtherBidCurrency;
    }
    if (browserSignals.adCost !== 3)
      throw 'Wrong adCost ' + browserSignals.adCost;
    if (browserSignals.reportingTimeout !== 2000)
    throw 'Wrong reportingTimeout ' + browserSignals.reportingTimeout;
  }
}

function validateSellerSignals(sellerSignals) {
  const sellerSignalsJson = JSON.stringify(sellerSignals);
  if (sellerSignalsJson !== '["component seller signals for winner"]')
    throw 'Wrong sellerSignals ' + sellerSignals;
}

function validateDirectFromSellerSignals(directFromSellerSignals) {
  const perBuyerSignalsJSON =
      JSON.stringify(directFromSellerSignals.perBuyerSignals);
  if (perBuyerSignalsJSON !== '{"from":"component","json":"for","buyer":[1]}' &&
      perBuyerSignalsJSON !== '{"buyer":[1],"from":"component","json":"for"}') {
    throw 'Wrong directFromSellerSignals.perBuyerSignals ' +
        perBuyerSignalsJSON;
  }
  const auctionSignalsJSON =
      JSON.stringify(directFromSellerSignals.auctionSignals);
  if (auctionSignalsJSON !==
          '{"from":"component","json":"for","all":["parties"]}' &&
      auctionSignalsJSON !==
          '{"all":["parties"],"from":"component","json":"for"}') {
    throw 'Wrong directFromSellerSignals.auctionSignals ' +
        auctionSignalsJSON;
  }
}
