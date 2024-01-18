(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Fetch intercepts CORS preflight requests correctly.`);

  const url = 'http://localhost:8000/inspector-protocol/fetch/resources/post-echo.pl';

  await dp.Network.enable();
  // Disable the cache so that we do not use cached OPTIONS.
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Fetch.enable();

  const contentPromise = session.evaluateAsync(`
      fetch("${url}", {method: 'POST', headers: {'X-DevTools-Test': 'foo'}, body: 'test'}).then(r => r.text())`);

  const request1 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`request 1: ${request1.request.method} ${request1.request.url}`);
  if (request1.request.method !== 'OPTIONS') {
    testRunner.log(`FAIL: preflight request expected`);
    testRunner.completeTest();
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
  });

  const request2 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`request 2: ${request2.request.method} ${request2.request.url}`);

  dp.Fetch.fulfillRequest({
    requestId: request2.requestId,
    responseCode: 200,
    responseHeaders: accessControlHeaders,
    body: btoa('response body')
  });

  testRunner.log(`response content: ${await contentPromise}`);
  testRunner.completeTest();
})
