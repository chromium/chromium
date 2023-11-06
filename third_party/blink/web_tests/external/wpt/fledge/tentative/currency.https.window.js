// META: script=/resources/testdriver.js
// META: script=/common/utils.js
// META: script=resources/fledge-util.sub.js
// META: script=/common/subset-tests.js
// META: timeout=long
// META: variant=?1-4
// META: variant=?5-8
// META: variant=?9-12
// META: variant=?13-16
// META: variant=?17-last

'use strict;'

const ORIGIN = window.location.origin;

// The tests in this file focus on calls to runAdAuction involving currency
// handling.

// Joins an interest group that bids 9USD on window.location.origin, and one
// that bids 10CAD on OTHER_ORIGIN1, each with a reportWin() report.
async function joinTwoCurrencyGroups(test, uuid) {
  const reportWinURL = createBidderReportURL(uuid, 'USD');
  const biddingURL = createBiddingScriptURL(
      {bidCurrency: 'USD', reportWin: `sendReportTo('${reportWinURL}')`});
  await joinInterestGroup(test, uuid, {biddingLogicURL: biddingURL});

  const otherReportWinURL = createBidderReportURL(uuid, 'CAD', OTHER_ORIGIN1);
  const otherBiddingURL = createBiddingScriptURL({
    origin: OTHER_ORIGIN1,
    bid: 10,
    bidCurrency: 'CAD',
    reportWin: `sendReportTo('${otherReportWinURL}')`
  });
  await joinCrossOriginInterestGroup(
      test, uuid, OTHER_ORIGIN1, {biddingLogicURL: otherBiddingURL});
}

function createBiddingScriptURLWithCurrency(uuid, currency) {
  return createBiddingScriptURL({
    bidCurrency: currency,
    allowComponentAuction: true,
    reportWin: `
        sendReportTo('${createBidderReportURL(uuid, /*id=*/ '')}' +
                     browserSignals.bid + encodeURIComponent(browserSignals.bidCurrency));`,
  });
}

function createDecisionURLExpectCurrency(uuid, currencyInScore) {
  return createDecisionScriptURL(uuid, {
    scoreAd: `
            if (browserSignals.bidCurrency !== '${currencyInScore}')
              throw 'Wrong currency';`,
    reportResult: `
            sendReportTo('${createSellerReportURL(uuid, /*id=*/ '')}' +
                         browserSignals.bid + encodeURIComponent(browserSignals.bidCurrency)   );`,
  });
}

async function runCurrencyComponentAuction(test, uuid, params = {}) {
  let auctionConfigOverrides = {
    interestGroupBuyers: [],
    decisionLogicURL: createDecisionScriptURL(uuid, {
      reportResult: `
        sendReportTo('${createSellerReportURL(uuid, 'top_')}' +
                     browserSignals.bid + encodeURIComponent(browserSignals.bidCurrency))`,
      ...params.topLevelSellerScriptParamsOverride
    }),
    componentAuctions: [{
      seller: ORIGIN,
      decisionLogicURL: createDecisionScriptURL(uuid, {
        reportResult: `
          sendReportTo('${createSellerReportURL(uuid, 'component_')}' +
                       browserSignals.bid + encodeURIComponent(browserSignals.bidCurrency))`,
        ...params.componentSellerScriptParamsOverride
      }),
      interestGroupBuyers: [ORIGIN],
      ...params.componentAuctionConfigOverrides
    }],
    ...params.topLevelAuctionConfigOverrides
  };
  return await runBasicFledgeAuction(test, uuid, auctionConfigOverrides);
}

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURL({bidCurrency: 'usd'})});
  await runBasicFledgeTestExpectingNoWinner(test, uuid);
}, 'Returning bid with invalid currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  await runBasicFledgeAuctionAndNavigate(
      test, uuid,
      {decisionLogicURL: createDecisionURLExpectCurrency(uuid, 'USD')});
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, '9???'), createBidderReportURL(uuid, '9???')
  ]);
}, 'Returning bid with currency, configuration w/o currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, undefined)});
  await runBasicFledgeAuctionAndNavigate(test, uuid, {
    perBuyerCurrencies: {'*': 'USD'},
    decisionLogicURL: createDecisionURLExpectCurrency(uuid, '???')
  });
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, '9USD'), createBidderReportURL(uuid, '9USD')
  ]);
}, 'Returning bid w/o currency, configuration w/currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  await runBasicFledgeAuctionAndNavigate(test, uuid, {
    perBuyerCurrencies: {'*': 'USD'},
    decisionLogicURL: createDecisionURLExpectCurrency(uuid, 'USD')
  });
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, '9USD'), createBidderReportURL(uuid, '9USD')
  ]);
}, 'Returning bid w/currency, configuration w/matching currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURL({bidCurrency: 'USD'})});
  await runBasicFledgeTestExpectingNoWinner(
      test, uuid, {perBuyerCurrencies: {'*': 'CAD'}});
}, 'Returning bid w/currency, configuration w/different currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinTwoCurrencyGroups(test, uuid);
  let auctionConfigOverrides = {
    interestGroupBuyers: [ORIGIN, OTHER_ORIGIN1],
    perBuyerCurrencies: {}
  };
  auctionConfigOverrides.perBuyerCurrencies['*'] = 'USD';
  auctionConfigOverrides.perBuyerCurrencies[OTHER_ORIGIN1] = 'CAD';
  await runBasicFledgeAuctionAndNavigate(test, uuid, auctionConfigOverrides);

  // Since the scoring script doesn't actually look at the currencies,
  // We expect 10CAD to win because 10 > 9
  await waitForObservedRequests(uuid, [
    createBidderReportURL(uuid, 'CAD', OTHER_ORIGIN1),
    createSellerReportURL(uuid)
  ]);
}, 'Different currencies for different origins, all match.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinTwoCurrencyGroups(test, uuid);
  let auctionConfigOverrides = {
    interestGroupBuyers: [ORIGIN, OTHER_ORIGIN1],
    perBuyerCurrencies: {}
  };
  auctionConfigOverrides.perBuyerCurrencies[ORIGIN] = 'USD';
  auctionConfigOverrides.perBuyerCurrencies[OTHER_ORIGIN1] = 'EUR';
  await runBasicFledgeAuctionAndNavigate(test, uuid, auctionConfigOverrides);

  // Since the configuration for CAD script expects EUR only the USD bid goes
  // through.
  await waitForObservedRequests(
      uuid, [createBidderReportURL(uuid, 'USD'), createSellerReportURL(uuid)]);
}, 'Different currencies for different origins, USD one matches.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinTwoCurrencyGroups(test, uuid);
  let auctionConfigOverrides = {
    interestGroupBuyers: [ORIGIN, OTHER_ORIGIN1],
    perBuyerCurrencies: {}
  };
  auctionConfigOverrides.perBuyerCurrencies['*'] = 'EUR';
}, 'Different currencies for different origins, none match.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let config = await runCurrencyComponentAuction(test, uuid, {
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
              if (browserSignals.bidCurrency !== 'USD')
                throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  // While scoring sees the original currency tag, reporting currency tags are
  // config-based.
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_9???'),
    createSellerReportURL(uuid, 'component_9???'),
    createBidderReportURL(uuid, '9???')
  ]);
}, 'Multi-seller auction --- no currency restriction.');


subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let config = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {sellerCurrency: 'USD'},
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
                if (browserSignals.bidCurrency !== 'USD')
                  throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  // Because component's sellerCurrency is USD, the bid it makes is seen to be
  // in dollars by top-level reporting. That doesn't affect reporting in its
  // own auction.
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_9USD'),
    createSellerReportURL(uuid, 'component_9???'),
    createBidderReportURL(uuid, '9???')
  ]);
}, 'Multi-seller auction --- component sellerCurrency matches bid.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let config = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {sellerCurrency: 'EUR'},
    componentSellerScriptParamsOverride: {
      scoreAd: `
        return {desirability: 2 * bid, allowComponentAuction: true,
                 bid: 1.5 * bid, bidCurrency: 'EUR'}
      `
    },
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
                if (browserSignals.bidCurrency !== 'EUR')
                  throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  // Because component's sellerCurrency is USD, the bid it makes is seen to be
  // in dollars by top-level reporting. That doesn't affect reporting in its
  // own auction.
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_13.5EUR'),
    createSellerReportURL(uuid, 'component_9???'),
    createBidderReportURL(uuid, '9???')
  ]);
}, 'Multi-seller auction --- component scoreAd modifies bid into its sellerCurrency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let config = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {sellerCurrency: 'EUR'},
    componentSellerScriptParamsOverride: {
      scoreAd: `
        return {desirability: 2 * bid, allowComponentAuction: true,
                 bid: 1.5 * bid}
      `
    },
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
                // scoreAd sees what's actually passed in.
                if (browserSignals.bidCurrency !== '???')
                  throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_13.5EUR'),
    createSellerReportURL(uuid, 'component_9???'),
    createBidderReportURL(uuid, '9???')
  ]);
}, 'Multi-seller auction --- component scoreAd modifies bid, no explicit currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let config = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides:
        {sellerCurrency: 'EUR', perBuyerCurrencies: {'*': 'USD'}},
    componentSellerScriptParamsOverride: {
      scoreAd: `
        return {desirability: 2 * bid, allowComponentAuction: true,
                 bid: 1.5 * bid}
      `
    },
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
                // scoreAd sees what's actually passed in.
                if (browserSignals.bidCurrency !== '???')
                  throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_13.5EUR'),
    createSellerReportURL(uuid, 'component_9USD'),
    createBidderReportURL(uuid, '9USD')
  ]);
}, 'Multi-seller auction --- component scoreAd modifies bid, bidder has bidCurrency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let config = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {perBuyerCurrencies: {'*': 'USD'}},
    componentSellerScriptParamsOverride: {
      scoreAd: `
        return {desirability: 2 * bid, allowComponentAuction: true,
                 bid: 1.5 * bid}
      `
    },
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
                // scoreAd sees what's actually passed in.
                if (browserSignals.bidCurrency !== '???')
                  throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_13.5???'),
    createSellerReportURL(uuid, 'component_9USD'),
    createBidderReportURL(uuid, '9USD')
  ]);
}, 'Multi-seller auction --- only bidder currency specified.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let result = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {sellerCurrency: 'EUR'},
    componentSellerScriptParamsOverride: {
      scoreAd: `
        return {desirability: 2 * bid, allowComponentAuction: true,
                 bid: 1.5 * bid, bidCurrency: 'CAD'}
      `
    }
  });
  expectNoWinner(result);
}, 'Multi-seller auction --- component scoreAd modifies bid to wrong currency.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let topLevelConfigOverride = {perBuyerCurrencies: {}};
  topLevelConfigOverride.perBuyerCurrencies[ORIGIN] = 'USD';
  let config = await runCurrencyComponentAuction(test, uuid, {
    topLevelAuctionConfigOverrides: topLevelConfigOverride,
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
              if (browserSignals.bidCurrency !== 'USD')
                throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  // Because component is constrained by perBuyerCurrencies for it on top-level
  // to USD, the bid it makes is seen to be in dollars by top-level reporting.
  // That doesn't affect reporting in its own auction.
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_9USD'),
    createSellerReportURL(uuid, 'component_9???'),
    createBidderReportURL(uuid, '9???')
  ]);
}, 'Multi-seller auction --- top-level perBuyerCurrencies matches bid.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let topLevelConfigOverride = {perBuyerCurrencies: {}};
  topLevelConfigOverride.perBuyerCurrencies[ORIGIN] = 'USD';
  let config = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {sellerCurrency: 'USD'},
    topLevelAuctionConfigOverrides: topLevelConfigOverride,
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
              if (browserSignals.bidCurrency !== 'USD')
                throw 'Wrong currency';`
    }
  });
  expectSuccess(config);
  createAndNavigateFencedFrame(test, config);
  // Because component is constrained by perBuyerCurrencies for it on top-level
  // to USD, the bid it makes is seen to be in dollars by top-level reporting.
  // That doesn't affect reporting in its own auction.
  await waitForObservedRequests(uuid, [
    createSellerReportURL(uuid, 'top_9USD'),
    createSellerReportURL(uuid, 'component_9???'),
    createBidderReportURL(uuid, '9???')
  ]);
}, 'Multi-seller auction --- consistent sellerConfig and top-level perBuyerCurrencies.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let topLevelConfigOverride = {perBuyerCurrencies: {}};
  topLevelConfigOverride.perBuyerCurrencies[ORIGIN] = 'EUR';
  let result = await runCurrencyComponentAuction(test, uuid, {
    componentAuctionConfigOverrides: {sellerCurrency: 'USD'},
    topLevelAuctionConfigOverrides: topLevelConfigOverride,
    topLevelSellerScriptParamsOverride: {
      scoreAd: `
              if (browserSignals.bidCurrency !== 'USD')
                throw 'Wrong currency';`
    }
  });
  expectNoWinner(result);
}, 'Multi-seller auction --- inconsistent sellerConfig and top-level perBuyerCurrencies.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let topLevelConfigOverride = {perBuyerCurrencies: {}};
  topLevelConfigOverride.perBuyerCurrencies[ORIGIN] = 'EUR';

  let result = await runCurrencyComponentAuction(
      test, uuid, {componentAuctionConfigOverrides: topLevelConfigOverride});
  expectNoWinner(result);
}, 'Multi-seller auction --- top-level perBuyerCurrencies different from bid.');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  await joinInterestGroup(
      test, uuid,
      {biddingLogicURL: createBiddingScriptURLWithCurrency(uuid, 'USD')});
  let result = await runCurrencyComponentAuction(
      test, uuid, {componentAuctionConfigOverrides: {sellerCurrency: 'EUR'}});
  expectNoWinner(result);
}, 'Multi-seller auction --- component sellerCurrency different from bid.');


// TODO: highestScoringOtherBid bid stuff:
// --- combination of sellerCurrency on/off, different conversion cases.
// Rules on conversion. Not strictly highestScoringOtherBid, but it makes it
// easier to check.
//
// PrivateAggregation stuff.
