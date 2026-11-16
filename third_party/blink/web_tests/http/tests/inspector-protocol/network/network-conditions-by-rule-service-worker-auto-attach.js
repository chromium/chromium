(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Verify that Network.emulateNetworkConditionsByRule sent right after an ' +
      'installed service worker is restarted and auto-attached (before the ' +
      'worker is resumed) applies to requests made by the worker.');
  const swHelper = (await testRunner.loadScript(
      '../service-worker/resources/service-worker-helper.js'))(dp, session);

  // Install the service worker while nothing is attached to it. Once it is
  // stopped, DevTools no longer has an agent host for it, so the host created
  // when it starts again is the one for an already installed version.
  await swHelper.installSWAndWaitForActivated(
      '/inspector-protocol/network/resources/service-worker-fetch.js');

  // Other tests running in the same content_shell instance can leave
  // registrations behind, so only look at the ones of this test.
  const isTestWorkerURL = url =>
      url.includes('/inspector-protocol/network/resources/');
  const findTestVersion = event =>
      event.params.versions.find(version => isTestWorkerURL(version.scriptURL));

  const [registrationEvent, initialVersionEvent] = await Promise.all([
    dp.ServiceWorker.onceWorkerRegistrationUpdated(
        event => event.params.registrations.some(
            registration => isTestWorkerURL(registration.scopeURL))),
    dp.ServiceWorker.onceWorkerVersionUpdated(findTestVersion),
    dp.ServiceWorker.enable(),
  ]);
  const scopeURL =
      registrationEvent.params.registrations
          .find(registration => isTestWorkerURL(registration.scopeURL))
          .scopeURL;
  // The worker may already be stopped (e.g. idle timeout); then no further
  // "stopped" event would arrive after stopAllWorkers().
  if (findTestVersion(initialVersionEvent).runningStatus !== 'stopped') {
    await Promise.all([
      dp.ServiceWorker.onceWorkerVersionUpdated(
          event => findTestVersion(event)?.runningStatus === 'stopped'),
      dp.ServiceWorker.stopAllWorkers(),
    ]);
  }

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'service_worker');
  await dp.ServiceWorker.startWorker({scopeURL});
  const attached = await attachedPromise;
  const workerDp = session.createChild(attached.params.sessionId).protocol;

  // Mimic what automation clients do: send the commands as soon as the worker
  // is attached and then resume it, without waiting for any responses.
  const pending = [
    workerDp.Network.enable(),
    workerDp.Network.emulateNetworkConditionsByRule({
      matchedNetworkConditions: [{
        urlPattern: 'http://*:*/*hello-world.html*',
        offline: true,
        latency: 0,
        downloadThroughput: -1,
        uploadThroughput: -1,
      }],
    }),
    workerDp.Runtime.runIfWaitingForDebugger(),
  ];
  const [, emulateResponse] = await Promise.all(pending);
  testRunner.log(emulateResponse.error ||
                 'emulateNetworkConditionsByRule succeeded');

  session.evaluate(`
    window.fetchInWorker = async url => {
      const registration =
          await navigator.serviceWorker.getRegistration('${scopeURL}');
      return new Promise(resolve => {
        navigator.serviceWorker.onmessage = event => resolve(event.data);
        registration.active.postMessage(url);
      });
    };
    undefined;
  `);

  const matching = await session.evaluateAsync(
      `window.fetchInWorker('/inspector-protocol/network/resources/hello-world.html')`);
  testRunner.log(`Matching request: ${matching}`);

  const nonMatching = await session.evaluateAsync(
      `window.fetchInWorker('/inspector-protocol/network/resources/worker.js')`);
  testRunner.log(`Non-matching request: ${nonMatching}`);

  await session.evaluateAsync(`
    navigator.serviceWorker.getRegistration('${scopeURL}')
        .then(registration => registration.unregister())`);
  testRunner.completeTest();
});
