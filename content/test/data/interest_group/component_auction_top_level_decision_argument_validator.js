// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals, directFromSellerSignals) {
  validateAdMetadata(adMetadata);
  validateBid(bid);
  validateAuctionConfig(auctionConfig);
  validateTrustedScoringSignals(trustedScoringSignals);
  validateBrowserSignals(browserSignals,/*isScoreAd=*/true);
  validateDirectFromSellerSignals(directFromSellerSignals);

  if (browserSignals.biddingDurationMsec < 0)
    throw 'Wrong biddingDurationMsec ' + browserSignals.biddingDurationMsec;

  return {desirability: 37, allowComponentAuction: true};
}

function reportResult(auctionConfig, browserSignals, directFromSellerSignals) {
  validateAuctionConfig(auctionConfig);
  validateBrowserSignals(browserSignals, /*isScoreAd=*/false);
  validateDirectFromSellerSignals(directFromSellerSignals);

  sendReportTo(auctionConfig.seller + '/echo?report_top_level_seller');
  return ['top-level seller signals for winner'];
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !== '["Replaced metadata"]') {
    throw 'Wrong adMetadata ' + adMetadataJSON;
  }
}

function validateBid(bid) {
  if (bid !== 42)
    throw 'Wrong bid ' + bid;
}

function validateAuctionConfig(auctionConfig) {
  if (Object.keys(auctionConfig).length !== 15) {
    throw 'Wrong number of auctionConfig fields ' +
        JSON.stringify(auctionConfig);
  }

  if (!auctionConfig.seller.includes('b.test'))
    throw 'Wrong seller ' + auctionConfig.seller;

  if (auctionConfig.decisionLogicURL !==
      auctionConfig.seller + '/interest_group' +
          '/component_auction_top_level_decision_argument_validator.js') {
    throw 'Wrong decisionLogicURL ' + auctionConfig.decisionLogicURL;
  }

  if (auctionConfig.decisionLogicUrl !==
      auctionConfig.seller + '/interest_group' +
          '/component_auction_top_level_decision_argument_validator.js') {
    throw 'Wrong decisionLogicUrl ' + auctionConfig.decisionLogicUrl;
  }

  if (auctionConfig.trustedScoringSignalsURL !==
      auctionConfig.seller + '/interest_group/trusted_scoring_signals.json') {
      throw 'Wrong trustedScoringSignalsURL ' +
          auctionConfig.trustedScoringSignalsURL;
  }

  if (auctionConfig.trustedScoringSignalsUrl !==
      auctionConfig.seller + '/interest_group/trusted_scoring_signals.json') {
      throw 'Wrong trustedScoringSignalsUrl ' +
          auctionConfig.trustedScoringSignalsUrl;
  }

  if ('interestGroupBuyers' in auctionConfig) {
    throw 'Wrong interestGroupBuyers ' + auctionConfig.interestGroupBuyers;
  }
  // If auctionSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail.
  const auctionSignalsJson = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJson !== '["top-level auction signals"]')
    throw 'Wrong auctionSignals ' + auctionConfig.auctionSignalsJSON;
  const sellerSignalsJson = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJson !== '["top-level seller signals"]')
    throw 'Wrong sellerSignals ' + auctionConfig.sellerSignalsJSON;
  if (auctionConfig.sellerTimeout !== 30000)
    throw 'Wrong sellerTimeout ' + auctionConfig.sellerTimeout;

  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  if (!perBuyerSignalsJson.includes('a.test') ||
      !perBuyerSignalsJson.includes('["top-level buyer signals"]')) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }

  const perBuyerTimeoutsJson = JSON.stringify(auctionConfig.perBuyerTimeouts);
  if (!perBuyerTimeoutsJson.includes('a.test') ||
      !perBuyerTimeoutsJson.includes('11000') ||
      auctionConfig.perBuyerTimeouts['*'] != 15000) {
    throw 'Wrong perBuyerTimeouts ' + perBuyerTimeoutsJson;
  }

  const perBuyerCumulativeTimeoutsJson =
      JSON.stringify(auctionConfig.perBuyerCumulativeTimeouts);
  if (!perBuyerCumulativeTimeoutsJson.includes('a.test') ||
      !perBuyerCumulativeTimeoutsJson.includes('11100') ||
      auctionConfig.perBuyerCumulativeTimeouts['*'] != 15100) {
    throw 'Wrong perBuyerCumulativeTimeouts ' + perBuyerCumulativeTimeoutsJson;
  }

  if (auctionConfig.reportingTimeout !== 3000)
    throw 'Wrong reportingTimeout ' + auctionConfig.reportingTimeout;

  if (auctionConfig.perBuyerCurrencies[
          auctionConfig.componentAuctions[0].seller] !== 'CAD' ||
      auctionConfig.perBuyerCurrencies['*'] !== 'MXN') {
    throw 'Wrong perBuyerCurrencies ' +
        JSON.stringify(auctionConfig.perBuyerCurrencies);
  }

  const perBuyerPrioritySignalsJson =
      JSON.stringify(auctionConfig.perBuyerPrioritySignals);
  if (Object.keys(auctionConfig.perBuyerPrioritySignals).length !== 1 ||
      JSON.stringify(auctionConfig.perBuyerPrioritySignals['*']) !==
          '{"foo":3}') {
    throw 'Wrong perBuyerTimeouts ' + perBuyerPrioritySignalsJson;
  }

  // Check componentAuctions. Don't check all fields of the expected component
  // auction, just a sampling of them.
  if (!('componentAuctions' in auctionConfig))
    throw 'componentAuctions missing';
  if (auctionConfig.componentAuctions.length != 1)
    throw 'Wrong componentAuctions ' + JSON.stringify(componentAuctions);
  const componentAuction = auctionConfig.componentAuctions[0];
  if (!componentAuction.seller.startsWith('https://d.test') ||
      componentAuction.decisionLogicURL != componentAuction.seller +
          '/interest_group' +
          '/component_auction_component_decision_argument_validator.js' ||
      componentAuction.decisionLogicUrl != componentAuction.seller +
          '/interest_group' +
          '/component_auction_component_decision_argument_validator.js' ||
      componentAuction.trustedScoringSignalsURL !== componentAuction.seller +
          '/interest_group/trusted_scoring_signals2.json' ||
      componentAuction.trustedScoringSignalsUrl !== componentAuction.seller +
          '/interest_group/trusted_scoring_signals2.json' ||
      componentAuction.sellerTimeout !== 20000 ||
      componentAuction.reportingTimeout !== 2000) {
    throw 'Wrong componentAuction ' + JSON.stringify(componentAuction);
  }
}

function validateTrustedScoringSignals(signals) {
  if (signals.renderURL["https://example.com/render"] !== "foo") {
    throw 'Wrong trustedScoringSignals.renderURL ' +
        signals.renderURL["https://example.com/render"];
  }
  if (signals.adComponentRenderURLs["https://example.com/render-component"] !==
      1) {
    throw 'Wrong trustedScoringSignals.adComponentRenderURLs ' +
        signals.adComponentRenderURLs["https://example.com/render-component"];
  }
  if (signals.renderUrl["https://example.com/render"] !== "foo") {
    throw 'Wrong trustedScoringSignals.renderUrl ' +
        signals.renderUrl["https://example.com/render"];
  }
  if (signals.adComponentRenderUrls["https://example.com/render-component"] !==
      1) {
    throw 'Wrong trustedScoringSignals.adComponentRenderUrls ' +
        signals.adComponentRenderUrls["https://example.com/render-component"];
  }
}

// Used for both scoreAd() and reportResult().
function validateBrowserSignals(browserSignals, isScoreAd) {
  // Fields common to scoreAd() and reportResult().
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if ('topLeverSeller' in browserSignals)
    throw 'Wrong topLeverSeller ' + browserSignals.topLeverSeller;
  if (!browserSignals.componentSeller.startsWith('https://d.test'))
    throw 'Wrong componentSeller ' + browserSignals.componentSeller;
  if (!browserSignals.interestGroupOwner.startsWith('https://a.test'))
    throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
  if (browserSignals.renderURL !== "https://example.com/render")
    throw 'Wrong renderURL ' + browserSignals.renderURL;
  if (browserSignals.renderUrl !== "https://example.com/render")
    throw 'Wrong renderUrl ' + browserSignals.renderUrl;
    if (browserSignals.bidCurrency !== 'CAD')
      throw 'Wrong bidCurrency ' + browserSignals.bidCurrency;

  // Fields that vary by method.
  if (isScoreAd) {
    if (Object.keys(browserSignals).length !== 10) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    const adComponentsJson = JSON.stringify(browserSignals.adComponents);
    if (adComponentsJson !== '["https://example.com/render-component"]')
      throw 'Wrong adComponents ' + adComponentsJson;
    if (browserSignals.biddingDurationMsec < 0)
      throw 'Wrong biddingDurationMsec ' + browserSignals.biddingDurationMsec;
    if (browserSignals.dataVersion !== 1234)
      throw 'Wrong dataVersion ' + browserSignals.dataVersion;
    if (browserSignals.forDebuggingOnlyInCooldownOrLockout)
      throw 'Wrong forDebuggingOnlyInCooldownOrLockout ' +
          browserSignals.forDebuggingOnlyInCooldownOrLockout;
  } else {
    if (Object.keys(browserSignals).length !== 11) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    validateBid(browserSignals.bid);
    if (browserSignals.desirability !== 37)
      throw 'Wrong desireability ' + browserSignals.desirability;
    if (browserSignals.highestScoringOtherBid !== 0) {
      throw 'Wrong highestScoringOtherBid ' +
          browserSignals.highestScoringOtherBid;
    }
    if (browserSignals.highestScoringOtherBidCurrency !== '???') {
      throw 'Wrong highestScoringOtherBidCurrency ' +
          browserSignals.highestScoringOtherBidCurrency;
    }
    if (browserSignals.dataVersion !== 1234)
      throw 'Wrong dataVersion ' + browserSignals.dataVersion;
  }
}

// Used for both scoreAd() and reportResult().
function validateDirectFromSellerSignals(directFromSellerSignals) {
  const sellerSignalsJSON =
      JSON.stringify(directFromSellerSignals.sellerSignals);
  if (sellerSignalsJSON !== '{"json":"for","the":["seller"]}') {
    throw 'Wrong directFromSellerSignals.sellerSignals ' +
        sellerSignalsJSON;
  }
  const auctionSignalsJSON =
      JSON.stringify(directFromSellerSignals.auctionSignals);
  if (auctionSignalsJSON !== '{"json":"for","all":["parties"]}' &&
      auctionSignalsJSON !== '{"all":["parties"],"json":"for"}') {
    throw 'Wrong directFromSellerSignals.auctionSignals ' +
        auctionSignalsJSON;
  }
}
