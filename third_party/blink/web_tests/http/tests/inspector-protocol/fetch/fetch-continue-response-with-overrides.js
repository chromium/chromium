(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test that Fetch.continueResponse without body parameter will use original response body`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Response'}]});

  session.navigate(testRunner.url('./resources/hello-world.html'))
  const requestPaused = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`Intercepted response ${requestPaused.responseStatusCode}`);
  const result = await dp.Fetch.continueResponse({
    requestId: requestPaused.requestId,
    responseCode: 201,
    responsePhrase: 'My status text',
    responseHeaders: requestPaused.responseHeaders,
  });
  if (result.error) {
    testRunner.log(`FAIL: couldn't continue response: ${JSON.stringify(result.error, null, 2)}`);
    testRunner.completeTest();
  }
  const responseReceived = (await dp.Network.onceResponseReceived()).params;
  testRunner.log(`Received response: ${responseReceived.response.status} ${responseReceived.response.statusText}`);
  await dp.Network.onceLoadingFinished();
  const body = await dp.Network.getResponseBody({ requestId: responseReceived.requestId });
  testRunner.log(`Received response body: ${body.result.body}`);
  testRunner.completeTest();
})
