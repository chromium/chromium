(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that service worker requests are intercepted when DevTools attached after start.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');

  let serviceWorkerSession;
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    serviceWorkerSession = session.createChild(event.params.sessionId);
  });

  await dp.ServiceWorker.enable();
  await session.navigate("resources/empty.html");
  session.evaluateAsync(`
      navigator.serviceWorker.register('service-worker.js?defer-install')`);

  async function waitForServiceWorkerPhase(phase) {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== phase);
    return versions[0];
  }

  const version = await waitForServiceWorkerPhase("installing");

  const url = 'fetch-data.txt';
  let content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response before interception enabled: ${content}`);

  const swFetcher = new FetchHelper(testRunner, serviceWorkerSession.protocol);
  swFetcher.setLogPrefix("[renderer] ");
  await swFetcher.enable();
  swFetcher.onRequest().fulfill({
    responseCode: 200,
    responseHeaders: [],
    body: btoa("overriden response body")
  });

  swFetcher.onceRequest(/service-worker-import\.js/).fulfill({
    responseCode: 200,
    responseHeaders: [
        {name: "content-type", value: "application/x-javascript"}],
    body: btoa(`self.imported_token = "overriden imported script!"`)
  });

  content = await serviceWorkerSession.evaluate(`
      importScripts("service-worker-import.js");
      self.imported_token
  `);
  testRunner.log(`Imported script after interception enabled: ${content}`);

  serviceWorkerSession.evaluate(`self.installCallback()`);
  await waitForServiceWorkerPhase("activated");
  await swFetcher.enable();

  dp.Page.reload();
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after interception enabled: ${content}`);

  // Stop worker and wait till it stopped, to make sure worker shutdown
  // is covered (see https://crbug.com/1306006).
  dp.ServiceWorker.stopWorker({versionId: version.versionId});
  await dp.ServiceWorker.onceWorkerVersionUpdated(
      e => e.params.versions.length && e.params.versions[0].runningStatus === "stopped"),

  testRunner.log("Stopped service worker");
  await serviceWorkerSession.disconnect();
  testRunner.log("Disconnected from service worker");
  testRunner.completeTest();
})
