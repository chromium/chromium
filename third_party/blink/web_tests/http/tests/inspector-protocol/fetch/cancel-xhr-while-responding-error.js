(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests to ensure error message on request cancelled while intercepting response.`);

  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Response'}]});

  session.evaluate(`
      window.xhr = new XMLHttpRequest();
      // This script will send headers then wait 10 seconds before sending body.
      window.xhr.open('GET', '/devtools/network/resources/resource.php?send=10000&nosniff=1', true);
      window.xhr.send();
  `);

  const request = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log('Request paused: ' + request.request.url.split('/').pop());

  session.evaluate(`window.xhr.abort();`);
  await dp.Network.onceLoadingFailed();

  // The mojo channel between renderer and the interception job is asynchronous
  // (i.e. not associated) WRT those between the render and the test client, and
  // test client and the interception job, so we have no sure way of knowing
  // when abortion takes effect on the intercept side. One way to find out is
  // poll by issuing a command that would otherwise be invalid in this state and
  // checking the error message -- if the job is still around, we should wait.
  for (let i = 0; i < 10; ++i) {
    const result = await dp.Fetch.continueWithAuth({
        requestId: request.requestId,
        authChallengeResponse: { response: 'Default'}
    });
    if (!/authChallengeResponse/.test(result.error.message))
      break;
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  testRunner.log('Renderer received abort signal');
  testRunner.log('Continuing intercepted request');
  const result = await dp.Fetch.continueRequest({requestId: request.requestId});
  testRunner.log('Error message: ' + result.error?.message);
  testRunner.completeTest();
})
