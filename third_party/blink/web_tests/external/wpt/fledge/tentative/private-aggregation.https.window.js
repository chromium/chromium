// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/common/utils.js
// META: script=resources/fledge-util.sub.js
// META: script=/common/subset-tests.js
// META: script=third_party/cbor-js/cbor.js
// META: timeout=long
// META: variant=?1-5

'use strict;'

// To better isolate from private aggregation tests run in parallel,
// don't use the usual origin here.
const MAIN_ORIGIN = OTHER_ORIGIN1;

const MAIN_PATH = '/.well-known/private-aggregation/report-protected-audience';
const DEBUG_PATH =
    '/.well-known/private-aggregation/debug/report-protected-audience';

const enableDebugMode = 'privateAggregation.enableDebugMode();';

// The next 3 methods are for interfacing with the test handler for
// Private Aggregation reports; adopted wholesale from Chrome-specific
// wpt_internal/private-aggregation/resources/utils.js
const resetReports = url => {
  url = `${url}?clear_stash=true`;
  const options = {
    method: 'POST',
    mode: 'no-cors',
  };
  return fetch(url, options);
};

const delay = ms => new Promise(resolve => step_timeout(resolve, ms));

async function pollReports(path, wait_for = 1, timeout = 5000 /*ms*/) {
  const targetUrl = new URL(path, MAIN_ORIGIN);
  const endTime = performance.now() + timeout;
  const outReports = [];

  do {
    const response = await fetch(targetUrl);
    assert_true(response.ok, 'pollReports() fetch response should be OK.');
    const reports = await response.json();
    outReports.push(...reports);
    if (outReports.length >= wait_for) {
      break;
    }
    await delay(/*ms=*/ 100);
  } while (performance.now() < endTime);

  return outReports.length ? outReports : null;
};

function decodeBase64(inStr) {
  let strBytes = atob(inStr);
  let arrBytes = new Uint8Array(strBytes.length);
  for (let i = 0; i < strBytes.length; ++i) {
    arrBytes[i] = strBytes.codePointAt(i);
  }
  return arrBytes.buffer;
}

function byteArrayToBigInt(inArray) {
  let out = 0n;
  for (let byte of inArray) {
    out = out * 256n + BigInt(byte);
  }
  return out;
}

async function getDebugSamples() {
  const debugReports = await pollReports(DEBUG_PATH);

  let samplesDict = new Map();

  // Extract samples for debug reports, and aggregate them, so we are not
  // reliant on how aggregation happens.
  for (let jsonReport of debugReports) {
    let report = JSON.parse(jsonReport);
    for (let payload of report.aggregation_service_payloads) {
      let decoded = CBOR.decode(decodeBase64(payload.debug_cleartext_payload));
      assert_equals(decoded.operation, 'histogram');
      for (let sample of decoded.data) {
        let convertedSample = {
          bucket: byteArrayToBigInt(sample.bucket),
          value: byteArrayToBigInt(sample.value)
        };
        if (convertedSample.value !== 0n) {
          let oldCount = 0n;
          if (samplesDict.has(convertedSample.bucket)) {
            oldCount = samplesDict.get(convertedSample.bucket);
          }

          samplesDict.set(
              convertedSample.bucket, oldCount + convertedSample.value);
        }
      }
    }
  }

  let samplesArray = [];
  for (let [bucket, value] of samplesDict.entries()) {
    // Stringify these so we can use assert_array_equals on them.
    samplesArray.push(bucket + ' => ' + value);
  }
  samplesArray.sort();
  return samplesArray;
}

function createIgOverrides(nameAndBid, fragments) {
  return {
    name: nameAndBid,
    biddingLogicURL: createBiddingScriptURL({
      origin: MAIN_ORIGIN,
      generateBid: enableDebugMode + fragments.generateBidFragment,
      reportWin: enableDebugMode + fragments.reportWinFragment,
      bid: nameAndBid
    })
  };
}

// Runs an auction with 2 interest groups, "1" and "2", with
// fragments.generateBidFragment/fragments.reportWinFragment/
// fragments.scoreAdFragment/fragments.reportResultFragment
// expected to make some Private Aggregation contributions.
// Returns the collected samples.
async function runPrivateAggregationTest(test, uuid, fragments) {
  await resetReports(MAIN_ORIGIN + '/' + MAIN_PATH);
  await resetReports(MAIN_ORIGIN + '/' + DEBUG_PATH);

  await joinCrossOriginInterestGroup(
      test, uuid, MAIN_ORIGIN, createIgOverrides('1', fragments));
  await joinCrossOriginInterestGroup(
      test, uuid, MAIN_ORIGIN, createIgOverrides('2', fragments));

  const auctionConfigOverrides = {
    decisionLogicURL: createDecisionScriptURL(uuid, {
      origin: MAIN_ORIGIN,
      scoreAd: enableDebugMode + fragments.scoreAdFragment,
      reportResult: enableDebugMode + fragments.reportResultFragment
    }),
    seller: MAIN_ORIGIN,
    interestGroupBuyers: [MAIN_ORIGIN]
  };

  await runBasicFledgeAuctionAndNavigate(test, uuid, auctionConfigOverrides);
  return await getDebugSamples();
}

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const fragments = {
    generateBidFragment: `
      privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,

    reportWinFragment:
        `privateAggregation.contributeToHistogram({ bucket: 2n, value: 3 });`,

    scoreAdFragment:
        `privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`,

    reportResultFragment:
        `privateAggregation.contributeToHistogram({ bucket: 4n, value: 5 });`
  };

  const samples = await runPrivateAggregationTest(test, uuid, fragments);
  assert_array_equals(
      samples,
      [
        '1 => 4',  // doubled since it's reported twice.
        '2 => 3',
        '3 => 8',  // doubled since it's reported twice.
        '4 => 5'
      ]);
}, 'Basic contributions');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const fragments = {
    generateBidFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always',
          { bucket: 1n, value: 2 });`,

    reportWinFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always',
          { bucket: 2n, value: 3 });`,

    scoreAdFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always',
          { bucket: 3n, value: 4 });`,

    reportResultFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always',
          { bucket: 4n, value: 5 });`
  };

  const samples = await runPrivateAggregationTest(test, uuid, fragments);
  assert_array_equals(
      samples,
      [
        '1 => 4',  // doubled since it's reported twice.
        '2 => 3',
        '3 => 8',  // doubled since it's reported twice.
        '4 => 5'
      ]);
}, 'reserved.always');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const fragments = {
    generateBidFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win',
          { bucket: 1n, value: interestGroup.name });`,

    reportWinFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win',
          { bucket: 2n, value: 3 });`,

    scoreAdFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win',
          { bucket: 3n, value: bid });`,

    reportResultFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win',
          { bucket: 4n, value: 5 });`
  };

  const samples = await runPrivateAggregationTest(test, uuid, fragments);
  assert_array_equals(
      samples,
      [
        '1 => 2',  // winning IG name
        '2 => 3',
        '3 => 2',  // winning bid
        '4 => 5'
      ]);
}, 'reserved.win');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const fragments = {
    generateBidFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss',
          { bucket: 1n, value: interestGroup.name });`,

    reportWinFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss',
          { bucket: 2n, value: 3 });`,

    scoreAdFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss',
          { bucket: 3n, value: bid });`,

    reportResultFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss',
          { bucket: 4n, value: 5 });`
  };

  const samples = await runPrivateAggregationTest(test, uuid, fragments);

  // No reserved.loss from reporting since they only run for winners.
  assert_array_equals(
      samples,
      [
        '1 => 1',  // losing IG name
        '3 => 1',  // losing bid
      ]);
}, 'reserved.loss');

subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const fragments = {
    generateBidFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.once',
          { bucket: 1n, value: interestGroup.name });`,

    reportWinFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.once',
          { bucket: 2n, value: 3 });`,

    scoreAdFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.once',
          { bucket: 3n, value: bid });`,

    reportResultFragment: `
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.once',
          { bucket: 4n, value: 5 });`
  };

  const samples = await runPrivateAggregationTest(test, uuid, fragments);

  // No reserved.once from reporting since it throws an exception.
  // bidder/scorer just pick one.
  assert_equals(samples.length, 2, 'samples array length');
  assert_in_array(samples[0], ['1 => 1', '1 => 2'], 'samples[0]');
  assert_in_array(samples[1], ['3 => 1', '3 => 2'], 'samples[1]');
  // TODO: explicitly test that reserved.once in reporting throws an exception.
}, 'reserved.once');
