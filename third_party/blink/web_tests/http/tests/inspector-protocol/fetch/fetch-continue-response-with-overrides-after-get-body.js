(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test that Fetch.continueResponse without body parameter will use original response body`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Response'}]});

  session.navigate(testRunner.url('./resources/hello-world.html'))
  const requestPaused = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`Intercepted response ${requestPaused.responseStatusCode}`);
  const response = await dp.Fetch.getResponseBody({ requestId: requestPaused.requestId });
  const interceptedBody = atob(response.result.body);
  testRunner.log(`Intercepted response body: '${interceptedBody}'`);
  const result = await dp.Fetch.continueResponse({
    requestId: requestPaused.requestId,
    responseCode: requestPaused.responseStatusCode,
    responsePhrase: 'My status text',
    responseHeaders: requestPaused.responseHeaders,
  });
  if (result.error) {
    testRunner.log(`FAIL: couldn't continue response: ${JSON.stringify(result.error, null, 2)}`);
    testRunner.completeTest();
  }
  const responseReceived = (await dp.Network.onceResponseReceived()).params;
  testRunner.log(`Finished navigation`);
  if ('My status text' === responseReceived.response.statusText)
    testRunner.log(`Received overridden status text`);
  else
    testRunner.log(`FAIL: did not override status text '${responseReceived.response.statusText}'`);
  await dp.Network.onceLoadingFinished();
  const body = await dp.Network.getResponseBody({ requestId: responseReceived.requestId });
  const responseBody = body.result.body;
  if (interceptedBody === responseBody)
    testRunner.log(`Response body is same as original: '${responseBody}'`);
  else
    testRunner.log(`FAIL: response body is different from original: '${responseBody}'`);
  testRunner.completeTest();
})
