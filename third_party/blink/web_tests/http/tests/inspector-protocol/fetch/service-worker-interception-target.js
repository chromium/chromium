(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that service worker script request is intercepted by SW target.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const rendererFetcher = new FetchHelper(testRunner, dp);
  rendererFetcher.setLogPrefix("[renderer] ");
  await rendererFetcher.enable();

  rendererFetcher.onRequest().continueRequest({});

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    const dp1 = session.createChild(event.params.sessionId).protocol;
    dp1.Runtime.enable();
    dp1.Runtime.onConsoleAPICalled(e => testRunner.log(`SW: ${e.params.args[0].value}`));
    const swFetcher = new FetchHelper(testRunner, dp1);
    swFetcher.setLogPrefix(`[${event.params.targetInfo.type}] `);
    await swFetcher.enable();
    swFetcher.onceRequest(/service-worker.js/).fulfill({
      responseCode: 200,
      responseHeaders: [{name: 'Content-type', value: 'application/x-javascript'},],
      body: btoa("console.log('PASS: intercepted')")
    });
    await dp1.Runtime.runIfWaitingForDebugger();
  });

  await dp.ServiceWorker.enable();
  await session.navigate("resources/empty.html");
  session.evaluateAsync(`navigator.serviceWorker.register('service-worker.js')`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
    return versions[0].registrationId;
  }

  await waitForServiceWorkerActivation();

  testRunner.completeTest();
})
