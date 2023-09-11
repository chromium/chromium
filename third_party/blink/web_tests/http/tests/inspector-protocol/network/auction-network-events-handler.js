(async function (testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const { session, dp } = await testRunner.startBlank(
    'Verifying the events that go through the auction network event handler are accurate.',
    { url: base + 'fledge_join.html?10' });

  await dp.Network.enable();

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

  dp.Network.onResponseReceived(async event => {
    const requestId = event.params.requestId;
    requestsById[requestId].received = true;
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

  const requests = Object.values(requestsById).sort((a, b) => a.url.localeCompare(b.url,"en"));
  testRunner.log(requests);

  testRunner.completeTest();
})
