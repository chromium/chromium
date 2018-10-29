(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/push-message-service-worker.html',
      `Tests delivery of a push message to the service worker.`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
    return versions[0].registrationId;
  }

  var registrationIdPromise = waitForServiceWorkerActivation();

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();
  const registrationId = await registrationIdPromise;

  dp.ServiceWorker.deliverPushMessage({origin: 'http://127.0.0.1:8000', registrationId: registrationId, data: 'push message'});
  const message = await session.evaluateAsync('window.__messagePromise');
  testRunner.log(`Got message: ${message}`);

  testRunner.completeTest();
});