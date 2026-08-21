(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that a request being intercepted while Fetch.disable arrives is not lost.`);

  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{requestStage: 'Request'}]});

  // A blob body has to be read asynchronously before the Fetch.requestPaused
  // notification can be sent, which is the window this test aims at: the
  // request has been picked up for interception, but the client has not been
  // told about it yet.
  const resultPromise = session.evaluateAsync(`
      (async () => {
        const body = new Blob([new Uint8Array(4 * 1024 * 1024)]);
        return await Promise.race([
          fetch('/devtools/network/resources/resource.php?size=10',
                {method: 'POST', body}).then(() => 'loaded', () => 'failed'),
          new Promise(resolve => setTimeout(() => resolve('hung'), 5000)),
        ]);
      })()
  `);

  await dp.Network.onceRequestWillBeSent();
  testRunner.log('Request started, disabling interception');
  await dp.Fetch.disable();
  testRunner.log('Result: ' + await resultPromise);
  testRunner.completeTest();
})
