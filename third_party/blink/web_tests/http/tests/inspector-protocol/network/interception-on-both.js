(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that a request can be intercepted on both request and response stages.`);

  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Network.setRequestInterception({patterns: [
    {urlPattern: '*', interceptionStage: 'Request'},
    {urlPattern: '*', interceptionStage: 'HeadersReceived'}
  ]});

  session.evaluate(`fetch('${testRunner.url('../network/resources/simple-iframe.html')}')`);

  const requestInterceptedPromise = dp.Network.onceRequestIntercepted();
  const requestSent = (await session.protocol.Network.onceRequestWillBeSent()).params.request;
  testRunner.log(`request will be sent: ${requestSent.url}`);

  const intercepted1 = (await requestInterceptedPromise).params;
  testRunner.log(`intercepted request: ${intercepted1.request.url}`);

  dp.Network.continueInterceptedRequest({interceptionId: intercepted1.interceptionId});

  const intercepted2 = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`intercepted response: ${intercepted2.request.url} ${intercepted2.responseStatusCode}`);
  dp.Network.continueInterceptedRequest({interceptionId: intercepted2.interceptionId});

  const responseReceived = (await session.protocol.Network.onceResponseReceived()).params.response;
  testRunner.log(`response received ${responseReceived.url}`);
  await dp.Network.onceLoadingFinished();
  testRunner.completeTest();
})
