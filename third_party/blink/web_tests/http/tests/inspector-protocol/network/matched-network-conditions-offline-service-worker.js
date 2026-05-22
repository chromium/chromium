(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Verify pattern matched network conditions offline emulation are applied to service worker main script in the main frame.`);

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();

  const registerPromise = session.evaluateAsync((workerUrl) => {
    return navigator.serviceWorker.register(workerUrl).then(
        () => 'service worker loaded successfully',
        () => 'service worker failed to load');
  }, testRunner.url('../service-worker/resources/blank-service-worker.js'));

  const attachedToTarget = await attachedPromise;
  const swSession = session.createChild(attachedToTarget.params.sessionId);
  const swdp = swSession.protocol;
  swdp.Network.enable();
  await swdp.Network.emulateNetworkConditionsByRule({
    matchedNetworkConditions: [{
      latency: 0,
      downloadThroughput: -1,
      uploadThroughput: -1,
      urlPattern: 'http://*:*/*blank-service-worker.js*',
      offline: true,
    }],
  });
  await new Promise(resolve => setTimeout(resolve, 1000));
  await swdp.Runtime.runIfWaitingForDebugger();

  const result = await registerPromise;

  testRunner.log('main frame service worker script load result: ' + result);

  testRunner.completeTest();
})
