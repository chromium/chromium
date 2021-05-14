// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals,
    browserSignals) {
  validateAdMetadata(adMetadata);
  validateBid(bid);
  validateAuctionConfig(auctionConfig);
  validateTrustedScoringSignals(trustedScoringSignals);
  validateBrowserSignals(browserSignals);
  return bid;
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !==
      '{"renderUrl":"https://example.com/render","metadata":{"ad":"metadata","here":[1,2,3]}}')
    throw 'Wrong adMetadata ' + adMetadataJSON;
}

function validateBid(bid) {
  if (bid !== 1)
    throw 'Wrong bid ' + bid;
}

function validateAuctionConfig(auctionConfig) {
  if (!auctionConfig.seller.includes('a.test'))
    throw 'Wrong seller ' + auctionConfig.seller;
  // TODO(crbug.com/1186444): Consider validating URL fields like
  // auctionConfig.decisionLogicUrl once we decide what to do about URL
  // normalization.
  // TODO(crbug.com/1186444): Test `trustedScoringSignals` once implemented.
  if (auctionConfig.interestGroupBuyers.length !== 1)
    throw 'Wrong interestGroupBuyers ' + auctionConfig.interestGroupBuyers;
  if (!auctionConfig.interestGroupBuyers[0].includes('a.test'))
    throw 'Wrong interestGroupBuyers ' + auctionConfig.interestGroupBuyers;
  // If auctionSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail. The order of properties produced by stringify() isn't guaranteed by
  // the ECMAScript standard, but some sites depend on the V8 behavior of
  // serializing in declaration order.
  const auctionSignalsJSON = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJSON !== '{"so":"I","hear":["you","like","json"]}')
    throw 'Wrong auctionSignals ' + auctionConfig.auctionSignalsJSON;
  const sellerSignalsJSON = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJSON !== '{"signals":"from","the":["seller"]}')
    throw 'Wrong sellerSignals ' + auctionConfig.sellerSignalsJSON;
  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  if (!perBuyerSignalsJson.includes('a.test') ||
      !perBuyerSignalsJson.includes('{"signalsForBuyer":1}')) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }
}

function validateTrustedScoringSignals(trustedScoringSignals) {
  // TODO(crbug.com/1186444): Test `trustedScoringSignals` once implemented.
}

function validateBrowserSignals(browserSignals) {
  if (!browserSignals.topWindowHostname.includes('a.test'))
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.interestGroupOwner.includes('a.test'))
    throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
  if (browserSignals.adRenderFingerprint === undefined ||
      browserSignals.adRenderFingerprint === '') {
    throw 'Wrong adRenderFingerprint ' + browserSignals.adRenderFingerprint;
  }
  if (browserSignals.biddingDurationMsec < 0)
    throw 'Wrong biddingDurationMsec ' + browserSignals.biddingDurationMsec;
}
