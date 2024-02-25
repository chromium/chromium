(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test interceptResponse parameter in redirected request interceptor`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Request'}, {requestStage: 'Response'}]});

  function isResponse(params) {
    return "responseErrorReason" in params || "responseStatusCode" in params;
  }

  const navigatePromise = session.navigate(testRunner.url('../resources/redirect1.php'))
  let requestNumber = 0;
  dp.Fetch.onRequestPaused(async event => {
    const params = {
      requestId: event.params.requestId,
    };
    if (isResponse(event.params)) {
      testRunner.log(`Intercepted response ${event.params.responseStatusCode}`);
    } else {
      testRunner.log(`Intercepted request ${event.params.request.url}`);
      ++requestNumber;
      params.interceptResponse = requestNumber === 2;
      if (!params.interceptResponse)
        testRunner.log(`Will skip next response`);
    }
    await dp.Fetch.continueRequest(params);
  });

  await navigatePromise
  testRunner.log('Finished navigation');
  testRunner.completeTest();
})
