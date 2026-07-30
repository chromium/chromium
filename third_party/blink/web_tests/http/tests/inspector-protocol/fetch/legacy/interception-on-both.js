(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that a request can be intercepted on both request and response stages.`);

  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Fetch.enable({
    patterns: [
      {urlPattern: '*', requestStage: 'Request'},
      {urlPattern: '*', requestStage: 'Response'}
    ]
  });

  session.evaluate(`fetch('${
      testRunner.url('../../network/resources/simple-iframe.html')}')`);

  const requestInterceptedPromise = dp.Fetch.onceRequestPaused();
  const requestSent =
      (await session.protocol.Network.onceRequestWillBeSent()).params.request;
  testRunner.log(`request will be sent: ${requestSent.url}`);

  const intercepted1 = (await requestInterceptedPromise).params;
  testRunner.log(`intercepted request: ${intercepted1.request.url}`);

  dp.Fetch.continueRequest({requestId: intercepted1.requestId});

  const intercepted2 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`intercepted response: ${intercepted2.request.url} ${
      intercepted2.responseStatusCode}`);
  dp.Fetch.continueRequest({requestId: intercepted2.requestId});

  const responseReceived =
      (await session.protocol.Network.onceResponseReceived()).params.response;
  testRunner.log(`response received ${responseReceived.url}`);
  await dp.Network.onceLoadingFinished();
  testRunner.completeTest();
})
