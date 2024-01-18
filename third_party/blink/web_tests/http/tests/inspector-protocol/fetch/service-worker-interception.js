(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that service worker requests are intercepted.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const globalFetcher = new FetchHelper(testRunner, testRunner.browserP());
  globalFetcher.setLogPrefix("[browser] ");
  await globalFetcher.enable();

  globalFetcher.onRequest().continueRequest({});

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    const dp1 = session.createChild(event.params.sessionId).protocol;
    const swFetcher = new FetchHelper(testRunner, dp1);
    swFetcher.setLogPrefix("[renderer] ");
    await swFetcher.enable();
    swFetcher.onRequest().continueRequest({});
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
  const onLifecyclePromise = dp.Page.onceLifecycleEvent(event => event.params.name === 'load');
  await dp.Page.reload();
  await onLifecyclePromise;

  globalFetcher.onceRequest().fulfill({
    responseCode: 200,
    responseHeaders: [],
    body: btoa("overriden response body")
  });

  const url = 'fetch-data.txt';
  let content = await session.evaluateAsync(`fetch("${url}?fulfill-by-sw").then(r => r.text())`);
  testRunner.log(`Response fulfilled by service worker: ${content}`);
  content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after Fetch.fulfillRequest: ${content}`);
  testRunner.completeTest();
})
