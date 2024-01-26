(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that requests are completed if interception is disabled between request and response.`);

  const bp = testRunner.browserP();
  // Intercept at browser level too, so we can delay response to
  // after renderer fetch is disabled.
  await bp.Fetch.enable({patterns: [{requestStage: 'Response'}]});
  await dp.Fetch.enable({patterns: [{requestStage: 'Request'}, {requestStage: 'Response'}]});
  const contentPromise = session.evaluateAsync(`
      fetch('/devtools/network/resources/resource.php?size=10')
          .then(response => response.text())
  `);
  const requestEvent = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log('Request intercepted: ' + requestEvent.request.url.split('/').pop());
  testRunner.log('Continuing request');
  dp.Fetch.continueRequest({requestId: requestEvent.requestId});
  const responseEvent = (await bp.Fetch.onceRequestPaused()).params;
  // Now the original interception still waits for response here, disable it.
  testRunner.log('Disabling request interception')
  await dp.Fetch.disable();
  bp.Fetch.continueRequest({requestId: responseEvent.requestId});

  testRunner.log('Body:\n' + await contentPromise);

  testRunner.completeTest();
})
