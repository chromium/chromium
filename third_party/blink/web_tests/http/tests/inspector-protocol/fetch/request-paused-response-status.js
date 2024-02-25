(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test that Fetch.requestPaused at reponse stage contains response status text`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Response'}]});

  const navigatePromise = session.navigate(testRunner.url('./resources/hello-world.html'))
  dp.Fetch.onRequestPaused(async event => {
    const params = {
      requestId: event.params.requestId,
    };
    testRunner.log(`Intercepted response status code: ${event.params.responseStatusCode}`);
    testRunner.log(`Intercepted response status text: '${event.params.responseStatusText}'`);
    await dp.Fetch.continueRequest(params);
  });
  const [response] = await Promise.all([
    dp.Network.onceResponseReceived(),
    navigatePromise
  ]);
  testRunner.log(`Finished navigation`);
  testRunner.log(`Response status text: '${response.params.response.statusText}'`);
  testRunner.completeTest();
})
