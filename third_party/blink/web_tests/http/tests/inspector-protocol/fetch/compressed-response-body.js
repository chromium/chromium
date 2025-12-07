(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test that Fetch.getResponseBody returns decompressed response body`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Response'}]});

  session.navigate(testRunner.url('./resources/gzip-response.php'))
  const requestPaused = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`Intercepted response ${requestPaused.responseStatusCode}`);
  const response = await dp.Fetch.getResponseBody({ requestId: requestPaused.requestId });
  const interceptedBody = atob(response.result.body);
  testRunner.log(`Intercepted response body: ${interceptedBody}`);
  await dp.Fetch.continueResponse({
    requestId: requestPaused.requestId,
  });
  await dp.Network.onceLoadingFinished();
  const receivedBody = await session.evaluate('document.body.innerHTML')
  testRunner.log(`Received response body: ${receivedBody}`);
  testRunner.completeTest();
})
