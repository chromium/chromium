// META: script=/resources/testdriver.js
// META: script=/common/utils.js
// META: script=resources/fledge-util.sub.js
// META: script=/common/subset-tests.js
// META: timeout=long
// META: variant=?1-2
// META: variant=?3-last

"use strict;"

const SERVICE_WORKER_SCRIPT = "service-worker-helper.js";

async function setUpServiceWorkerAndGetBroadcastChannel(broadcastChannelName) {
  await registerServiceWorker();
  await checkServiceWorkersAndReload();
  return new BroadcastChannel(broadcastChannelName);
}

// In order to validate a service worker, its scriptURL should
// be the one we registered in registerServiceWorker().
function validateServiceWorker() {
  if (navigator.serviceWorker.controller) {
    return navigator.serviceWorker.controller.scriptURL.includes(SERVICE_WORKER_SCRIPT);
  }
  return false;
}

// Prepares the test environment by ensuring a valid service worker
// registration. Reloads the page if no service worker is found or
// if it's not registered correctly.
async function checkServiceWorkersAndReload() {
  let count = (await navigator.serviceWorker.getRegistrations()).length;
  if (count === 0 || !validateServiceWorker()) {
    window.location.reload();
  }
}

// Registers a service worker to the current scope.
async function registerServiceWorker() {
  try {
    await navigator.serviceWorker.register(`./${SERVICE_WORKER_SCRIPT}`);
    await navigator.serviceWorker.ready;
  } catch (error) {
    throw (Error, "Error while registering service worker: " + error);
  }
}

// Tests that public requests are seen by the service worker.
// Specifically anything that contains:
// - 'direct-from-seller-signals.py'

// This test works by having the service worker send a message over
// the broadcastChannel, if it sees a request that contains any of
// the following strings above, it will send a 'passed' result and
// also change the variable 'finish_test', to true, so that guarantees
// that the request was seen before we complete the test.
subsetTest(promise_test, async test => {
  const broadcastChannel = await setUpServiceWorkerAndGetBroadcastChannel('public-requests-test');

  let finishTest = new Promise((resolve, reject) => {
    broadcastChannel.addEventListener('message', (event) => {
      if (event.data.result === 'passed') {
        resolve();
      } else {
        reject(`unexpected result: ${event.data.message}`);
      }
    });
  });

  await fetchDirectFromSellerSignals({ 'Buyer-Origin': window.location.origin });
  await finishTest;
}, "Make sure service workers do see public requests.");

// Tests that private requests are not seen by the service worker.
// Specifically anything that contains:
// - 'resources/trusted-bidding-signals.py'
// - 'resources/trusted-scoring-signals.py'
// - 'wasm-helper.py'
// - 'bidding-logic.py'
// - 'decision-logic.py'
// - 'seller_report'
// - 'bidder_report'

// This test works by having the service worker send a message
// over the broadcastChannel, if it sees a request that contains
// any of the following strings above, it will send a 'failed'
// result which will cause assert_false case to fail.
subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const broadcastChannel = await setUpServiceWorkerAndGetBroadcastChannel('private-requests-test');

  let finishTest = new Promise((resolve, reject) => {
    broadcastChannel.addEventListener('message', (event) => {
      if (event.data.result === 'completed') {
        resolve();
      } else {
        reject(`unexpected result: ${event.data.message}`);
      }
    });
  });
  broadcastChannel.addEventListener('message', (event) => {
    assert_false(event.data.result === 'failed',
    /*errorMessage=*/event.data.message);
  });
  let interestGroupOverrides = {
    biddingWasmHelperURL: `${RESOURCE_PATH}wasm-helper.py`,
    trustedBiddingSignalsURL: TRUSTED_BIDDING_SIGNALS_URL,
    trustedScoringSignalsURL: TRUSTED_SCORING_SIGNALS_URL,
  };

  await joinInterestGroup(test, uuid, interestGroupOverrides);
  await runBasicFledgeAuctionAndNavigate(test, uuid);
  // By verifying that these requests are observed we can assume
  // none of the other requests were seen by the service-worker.
  await waitForObservedRequests(
    uuid,
    [createBidderReportURL(uuid), createSellerReportURL(uuid)]);

  // Service worker will post a 'completed' result when it sees a fetch request with
  // 'COMPLETE-TEST' within the URL. This let's us ensure that the service worker's
  // 'postMessage' has nothing pending.
  await fetch('COMPLETE-TEST');
  await finishTest;
}, "Make sure service workers do not see private requests");

// Tests that private requests are not seen by the service worker.
// Specifically the `update-url.py`.
subsetTest(promise_test, async test => {
  const uuid = generateUuid(test);
  const broadcastChannel = await setUpServiceWorkerAndGetBroadcastChannel('private-requests-test');

  let finishTest = new Promise((resolve, reject) => {
    broadcastChannel.addEventListener('message', (event) => {
      if (event.data.result === 'completed') {
        resolve();
      } else {
        reject(`unexpected result: ${event.data.message}`);
      }
    });
  });
  broadcastChannel.addEventListener('message', (event) => {
    assert_false(event.data.result === 'failed',
    /*errorMessage=*/event.data.message);
  });
  const trackedUpdateURL = createTrackerURL(origin, uuid, `track_get`,/*id=*/`update-url.py`);

  await joinInterestGroupWithoutDefaults(test,{
    owner: origin,
    name: DEFAULT_INTEREST_GROUP_NAME,
    updateURL: trackedUpdateURL,
  })
  await runBasicFledgeAuction(test, uuid);

  // By verifying that these requests are observed we can assume
  // none of the other requests were seen by the service-worker.
  await waitForObservedRequests(
    uuid,
    [trackedUpdateURL]);

  // Service worker will post a 'completed' result when it sees a fetch request with
  // 'COMPLETE-TEST' within the URL. This let's us ensure that the service worker's
  // 'postMessage' has nothing pending.
  await fetch('COMPLETE-TEST');
  await finishTest;
}, "Make sure service workers do not see private requests, specifically updateURL");
