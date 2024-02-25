(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests interoperation of Fetch interception with Network instrumentation for CORS preflight requests.`);

  const url = 'http://localhost:8000/inspector-protocol/fetch/resources/post-echo.pl';

  const protocolMessages = [];
  const originalDispatchMessage = DevToolsAPI.dispatchMessage;
  DevToolsAPI.dispatchMessage = (message) => {
    protocolMessages.push(message);
    originalDispatchMessage(message);
  };

  await dp.Network.enable();
  // Disable the cache so that we do not use cached OPTIONS.
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Fetch.enable();

  session.evaluate(`
      contentPromise = fetch("${url}", {method: 'POST', headers: {'X-DevTools-Test': 'foo'}, body: 'test'}).then(r => r.text())`);

  const eventsById = new Map();
  function onNetworkEvent(event) {
    let eventList = eventsById.get(event.params.requestId);
    if (!eventList) {
      eventList = [];
      eventsById.set(event.params.requestId, eventList)
    }
    eventList.push(event.method);
  }
  dp.Network.onRequestWillBeSent(onNetworkEvent);
  dp.Network.onResponseReceived(onNetworkEvent);
  dp.Network.onLoadingFinished(onNetworkEvent);

  const request1 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`request 1: ${request1.request.method} ${request1.request.url} networkId: ${typeof request1.networkId}`);
  if (request1.request.method !== 'OPTIONS') {
    testRunner.log(protocolMessages);
    testRunner.fail(`FAIL: preflight request expected`);
    return;
  }
  const accessControlHeaders =  [
    {name: 'Access-Control-Allow-Origin', value: 'http://127.0.0.1:8000'},
    {name: 'Access-Control-Allow-Methods', value: 'POST, OPTIONS, GET'},
    {name: 'Access-Control-Allow-Headers', value: '*'},
  ];
  dp.Fetch.fulfillRequest({
    requestId: request1.requestId,
    responseCode: 204,
    responseHeaders: accessControlHeaders,
  });``
  const request2 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`request 2: ${request2.request.method} ${request2.request.url} networkId: ${typeof request2.networkId}`);

  dp.Fetch.fulfillRequest({
    requestId: request2.requestId,
    responseCode: 200,
    responseHeaders: accessControlHeaders,
    body: btoa('response body')
  });

  await session.evaluateAsync('contentPromise');
  testRunner.log(eventsById.get(request1.networkId), "Preflight request network events: ");
  testRunner.log(eventsById.get(request2.networkId), "Actual request network events: ");
  testRunner.completeTest();
})

