(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test interceptResponse=false parameter in request interceptor`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Request'}, {requestStage: 'Response'}]});

  function isResponse(params) {
    return "responseErrorReason" in params || "responseStatusCode" in params;
  }

  const navigatePromise = session.navigate(testRunner.url('./resources/hello-world.html'))
  dp.Fetch.onRequestPaused(async event => {
    if (isResponse(event.params))
      testRunner.fail(`Unexpected Fetch.requestPaused event for response`);
    else
      testRunner.log(`Intercepted request`);
    const {error} = await dp.Fetch.continueRequest({
      requestId: event.params.requestId,
      interceptResponse: false
    });
  });

  const result = await navigatePromise
  testRunner.log('Finished navigation');
  testRunner.completeTest();
})
