const publicKeyConfig = `{
  "originScopedKeys": {
    "https://a.test:8443": {
      "keys":[{
        "id":"14345678-9abc-def0-1234-56789abcdef0",
        "key":"oV9AZYb6xHuZWXDxhdnYkcdNzx65Gn1QpYsBaD5gBS0="}]
    }
  }
}`;

// 32 random'ish bits in hex.
function rand32() {
  return Math.abs((Math.random() * 0x100000000) & 0xFFFFFFFF).toString(16)
}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'

  async function runTest(useCoordinator) {
    let finishedLoadingRequests = 0;
    let finishedReceivingExtraInfo = 0;
    let resolveAllRequestsCompleted = null;
    const allRequestsCompleted = new Promise((resolve) => {
      resolveAllRequestsCompleted = resolve;
    });

    let coordinator;
    if (useCoordinator) {
      // We generate a random coordinator hostname to isolate tests.
      coordinator = "https://cd" + rand32() + rand32() + rand32() + rand32() +
                    ".test";
    }

    const { session, dp } = await testRunner.startBlank(
      `Verifying the events that go through the auction network event handler are accurate with coordinator: ${useCoordinator}.\n` +
      `Note: There are two bidding requests because we reload the bidding worklet when we are reporting.`,
      { url: base + `fledge_join.html?count=1&useTrustedBiddingSignals=true` +
          (useCoordinator ? `&coordinator=${coordinator}` : '') });

    await dp.Network.enable();
    await dp.Network.setCacheDisabled({ cacheDisabled: true });

    if (useCoordinator) {
      await dp.Browser.addPrivacySandboxCoordinatorKeyConfig({
        api: 'TrustedKeyValue',
        coordinatorOrigin: coordinator,
        keyConfig: publicKeyConfig
      });
    }

    await dp.Emulation.setUserAgentOverride({ userAgent: 'Vending Machine', acceptLanguage: 'ar' });

    function checkIfAllRequestsCompleted() {
      if (finishedLoadingRequests > 6 && finishedReceivingExtraInfo > 6) {
        resolveAllRequestsCompleted(true);
      }
    }

    const requestsById = new Map();

    dp.Network.onRequestWillBeSent(event => {
      const request = event.params.request;
      requestsById[event.params.requestId] = {
        method: request.method,
        url: request.url,
        headers: request.headers,
        received: false,
        finished: false,
      };
    });

    dp.Network.onRequestWillBeSentExtraInfo(async event => {
      const requestId = event.params.requestId;
      requestsById[requestId].requestExtraInfoReceived = true;
      requestsById[requestId].headers.userAgent = event.params.headers["User-Agent"];
      requestsById[requestId].headers.acceptLanguage = event.params.headers["Accept-Language"];
      if (event.params.headers["Cache-Control"] == "no-cache") {
        requestsById[requestId].cacheDisabled = true;
      }
      if (event.params.connectTiming) {
        requestsById[requestId].requestHasTiming = true;
      }
    });

    dp.Network.onResponseReceived(async event => {
      const requestId = event.params.requestId;
      requestsById[requestId].received = true;
      if (event.params.response.timing) {
        requestsById[requestId].responseHasTiming = true;
      }
    });

    dp.Network.onResponseReceivedExtraInfo(async event => {
      const requestId = event.params.requestId;
      requestsById[requestId].responseExtraInfoReceived = true;
      finishedReceivingExtraInfo++;
      checkIfAllRequestsCompleted();

    });

    dp.Network.onLoadingFinished(async event => {
      const requestId = event.params.requestId;
      requestsById[requestId].finished = true;
      finishedLoadingRequests++;
      checkIfAllRequestsCompleted();
    });

    const auctionJs = `
    async function runAdAuction() {
        const result = await navigator.runAdAuction({
          decisionLogicURL: "${base}fledge_decision_logic.js.php",
          trustedScoringSignalsURL: "${base}fledge_scoring_signals.json.php",
          trustedScoringSignalsCoordinator: ${JSON.stringify(coordinator)},
          seller: "https://a.test:8443",
          interestGroupBuyers: ["https://a.test:8443"],
          resolveToConfig: true
        });

        const fencedFrame = document.createElement('fencedframe');
        fencedFrame.mode = "opaque-ads";
        fencedFrame.config = result;
        document.body.appendChild(fencedFrame);
      };
      runAdAuction();
        `;

    await session.evaluateAsync(auctionJs);
    await allRequestsCompleted;

    const requests = Object.values(requestsById).sort((a, b) => a.url.localeCompare(b.url, "en"));
    testRunner.log(requests);
  }

  await runTest(/*useCoordinator=*/false);
  await runTest(/*useCoordinator=*/true);
  testRunner.completeTest();
})
