(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Fetch intercepts CORS preflight requests from service workers correctly.`);

  const url = 'http://localhost:8000/inspector-protocol/network/resources/post-echo.pl';
  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  let swSession;

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const accessControlHeaders =  [
    {name: 'Access-Control-Allow-Origin', value: 'http://127.0.0.1:8000'},
    {name: 'Access-Control-Allow-Methods', value: 'POST, OPTIONS, GET'},
    {name: 'Access-Control-Allow-Headers', value: '*'},
  ];

  dp.Target.onAttachedToTarget(async event => {
    swSession = session.createChild(event.params.sessionId);
    const swdp = swSession.protocol;
    const swFetcher = new FetchHelper(testRunner, swdp);
    swFetcher.setLogPrefix("[sw] ");
    await swFetcher.enable();
    swFetcher.onceRequest(/post-echo/).matched().then(request => {
      if (request.request.method !== 'OPTIONS') {
        testRunner.log(`FAILED: expecting OPTIONS request first, got ${request.request.method}`);
        testRunner.completeTest();
        return;
      }
      swdp.Fetch.fulfillRequest({
        requestId: request.requestId,
        responseCode: 204,
        responseHeaders: accessControlHeaders,
      });
    });
    swFetcher.onceRequest(/post-echo/).matched().then(request => {
      swdp.Fetch.fulfillRequest({
        requestId: request.requestId,
        responseCode: 200,
        responseHeaders: accessControlHeaders,
        body: btoa('response body')
      });
    });
    swFetcher.onRequest().continueRequest({});
    swdp.Runtime.runIfWaitingForDebugger();
  });

  await dp.ServiceWorker.enable();
  await session.navigate("resources/service-worker.html");
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
  dp.Page.reload();
  await onLifecyclePromise;
  const response = await session.evaluateAsync(`
      fetch("${url}", {method: 'POST', headers: {'X-DevTools-Test': 'foo'}, body: 'test'}).then(r => r.text())`);
  testRunner.log(`fetch response: ${response}`);
  testRunner.completeTest();
})
