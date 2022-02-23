// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  validateAdMetadata(adMetadata);
  validateBid(bid);
  validateAuctionConfig(auctionConfig);
  validateTrustedScoringSignals(trustedScoringSignals);
  validateBrowserSignals(browserSignals, /*isScoreAd=*/true);
  return 13;
}

function reportResult(auctionConfig, browserSignals) {
  validateAuctionConfig(auctionConfig);
  validateBrowserSignals(browserSignals, /*isScoreAd=*/false);

  sendReportTo(auctionConfig.seller + '/echo?report_component_seller');
  return ['component seller signals for winner'];
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !==
      '{"renderUrl":"https://example.com/render",' +
      '"metadata":{"ad":"metadata","here":[1,2,3]}}') {
    throw 'Wrong adMetadata ' + adMetadataJSON;
  }
}

function validateBid(bid) {
  if (bid !== 2)
    throw 'Wrong bid ' + bid;
}

function validateAuctionConfig(auctionConfig) {
  if (!auctionConfig.seller.includes('d.test'))
    throw 'Wrong seller ' + auctionConfig.seller;
  // TODO(crbug.com/1186444): Consider validating URL fields like
  // auctionConfig.decisionLogicUrl once we decide what to do about URL
  // normalization.
  if (auctionConfig.interestGroupBuyers.length !== 1 ||
      !auctionConfig.interestGroupBuyers[0].startsWith('https://a.test')) {
    throw 'Wrong interestGroupBuyers ' + auctionConfig.interestGroupBuyers;
  }
  // If auctionSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail.
  const auctionSignalsJson = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJson !== '["component auction signals"]')
    throw 'Wrong auctionSignals ' + auctionConfig.auctionSignals;
  const sellerSignalsJson = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJson !== '["component seller signals"]')
    throw 'Wrong sellerSignals ' + auctionConfig.sellerSignalsJson;
  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  if (!perBuyerSignalsJson.includes('a.test') ||
      !perBuyerSignalsJson.includes('["component buyer signals"]')) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }
  const perBuyerTimeoutsJson = JSON.stringify(auctionConfig.perBuyerTimeouts);
  if (!perBuyerTimeoutsJson.includes('a.test') ||
      !perBuyerTimeoutsJson.includes('200')) {
    throw 'Wrong perBuyerTimeouts ' + perBuyerTimeoutsJson;
  }
}

function validateTrustedScoringSignals(signals) {
  if (signals.renderUrl["https://example.com/render"] !== "bar") {
    throw 'Wrong trustedScoringSignals.renderUrl ' +
        signals.renderUrl["https://example.com/render"];
  }
  if (signals.adComponentRenderUrls["https://example.com/render-component"] !==
      2) {
    throw 'Wrong trustedScoringSignals.adComponentRenderUrls ' +
        signals.adComponentRenderUrls["https://example.com/render-component"];
  }
}

// Used for both scoreAd() and reportResult().
function validateBrowserSignals(browserSignals, isScoreAd) {
  // Fields common to scoreAd() and reportResult().
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.interestGroupOwner.startsWith('https://a.test'))
    throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
  if (browserSignals.renderUrl !== "https://example.com/render")
    throw 'Wrong renderUrl ' + browserSignals.renderUrl;

  // Fields that vary by method.
  if (isScoreAd) {
    if (Object.keys(browserSignals).length !== 6) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    const adComponentsJson = JSON.stringify(browserSignals.adComponents);
    if (adComponentsJson !== '["https://example.com/render-component"]')
      throw 'Wrong adComponents ' + adComponentsJson;
    if (browserSignals.biddingDurationMsec < 0)
      throw 'Wrong biddingDurationMsec ' + browserSignals.biddingDurationMsec;
    if (browserSignals.dataVersion !== 5678)
      throw 'Wrong dataVersion ' + browserSignals.dataVersion;
  } else {
    if (Object.keys(browserSignals).length !== 6) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    validateBid(browserSignals.bid);
    if (browserSignals.desirability !== 13)
      throw 'Wrong desireability ' + browserSignals.desirability;
    if (browserSignals.dataVersion !== 5678)
      throw 'Wrong dataVersion ' + browserSignals.dataVersion;
  }
}
