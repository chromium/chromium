(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test interceptResponse=true parameter in request interceptor`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Request'}, {requestStage: 'Response'}]});

  function isResponse(params) {
    return "responseErrorReason" in params || "responseStatusCode" in params;
  }

  const navigatePromise = session.navigate(testRunner.url('./resources/hello-world.html'))
  dp.Fetch.onRequestPaused(async event => {
    const params = {
      requestId: event.params.requestId,
    };
    if (isResponse(event.params)) {
      testRunner.log(`Intercepted response`);
    } else {
      testRunner.log(`Intercepted request`);
      params.interceptResponse = true;
    }
    await dp.Fetch.continueRequest(params);
  });

  const result = await navigatePromise
  testRunner.log('Finished navigation');
  testRunner.completeTest();
})
