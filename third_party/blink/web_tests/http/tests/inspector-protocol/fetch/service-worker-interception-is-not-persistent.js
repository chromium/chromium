(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      `Tests that service worker script is reloaded from disk after DevTools detach.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    const dp1 = session.createChild(event.params.sessionId).protocol;

    const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
    const fetchHelper = new FetchHelper(testRunner, dp1);
    await fetchHelper.enable();
    fetchHelper.onceRequest().fulfill(FetchHelper.makeContentResponse(
        `self.addEventListener('fetch', event => {
           event.respondWith(new Response('devtools intercepted'));
         });
         self.addEventListener('activate', event => {
           event.waitUntil(clients.claim());
         });`,
        'application/x-javascript'));
    await dp1.Runtime.runIfWaitingForDebugger();
  });

  await dp.ServiceWorker.enable();
  await session.navigate('resources/empty.html');
  session.evaluateAsync(
      `navigator.serviceWorker.register('service-worker.js')`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== 'activated');
    return versions[0].registrationId;
  }

  await waitForServiceWorkerActivation();
  await dp.Page.reload();
  // Verify the page content reflects the modified service worker script.
  let data = await session.evaluateAsync(
      `fetch("fetch-data.txt?fulfill-by-sw").then(r => r.text())`);
  testRunner.log(`Page content (after DevTools modification): "${data}"`);

  // Detach the root session (which would also detach child SW session).
  // This removes the Fetch override.
  await session.disconnect();
  session = await page.createSession();

  await session.navigate('resources/empty.html');
  data = await session.evaluateAsync(
      `fetch("fetch-data.txt?fulfill-by-sw").then(r => r.text())`);
  testRunner.log(`Page content (after reload): "${data}"`);

  testRunner.completeTest();
})
