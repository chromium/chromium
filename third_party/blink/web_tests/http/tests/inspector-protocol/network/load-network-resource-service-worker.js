(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
    'Tests that we can load resources from a service worker.');
  const swHelper = (await testRunner.loadScript('../service-worker/resources/service-worker-helper.js'))(dp, session);

  let swdp = null;
  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    swdp = session.createChild(event.params.sessionId).protocol;
  });

  const serviceWorkerURL = '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);

  swdp.Network.enable();
  const url = `http://localhost:8000/inspector-protocol/network/resources/source.map`;
  const response1 = await swdp.Network.loadNetworkResource(
      {url, options: {disableCache: false, includeCredentials: false}});
  testRunner.log(response1.result, `Response for fetch with existing resource: `, ["headers", "stream"]);

  await swdp.Inspector.enable();
  const [registration] = await Promise.all([
    dp.ServiceWorker.onceWorkerRegistrationUpdated(),
    dp.ServiceWorker.enable(),
  ]);
  const scopeURL = registration.params.registrations[0].scopeURL;
  await Promise.all([
    swdp.Inspector.onceTargetCrashed(),
    dp.ServiceWorker.stopAllWorkers(),
  ]);
  const response2 = await swdp.Network.loadNetworkResource(
      {url, options: {disableCache: false, includeCredentials: false}});
  testRunner.log(response2.error,
                 `Response for fetch from a stopped service worker: `);

  // Commands for the renderer wait until the service worker starts again.
  const bodyPromise = swdp.Network.getResponseBody({requestId: 'unknown'});
  await dp.ServiceWorker.startWorker({scopeURL});
  testRunner.log((await bodyPromise).error,
                 `Response body request sent to a stopped service worker: `);
  testRunner.completeTest();
});
