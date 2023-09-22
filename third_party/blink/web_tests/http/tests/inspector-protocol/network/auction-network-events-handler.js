(async function (testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const { session, dp } = await testRunner.startBlank(
    'Verifying the events that go through the auction network event handler are accurate.',
    { url: base + 'fledge_join.html?10' });

  await dp.Network.enable();
  await dp.Network.setCacheDisabled({ cacheDisabled: true });

  await dp.Emulation.setUserAgentOverride({ userAgent: 'Vending Machine', acceptLanguage: 'ar' });

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
  });

  dp.Network.onLoadingFinished(async event => {
    const requestId = event.params.requestId;
    requestsById[requestId].finished = true;
  });
  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicURL: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;

  await session.evaluateAsync(auctionJs);

  const requests = Object.values(requestsById).sort((a, b) => a.url.localeCompare(b.url, "en"));
  testRunner.log(requests);

  testRunner.completeTest();
})
