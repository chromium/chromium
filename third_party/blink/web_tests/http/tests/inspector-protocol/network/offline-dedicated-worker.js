(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that Network.emulateNetworkConditionsByRule on the worker session stops requests when offline is enabled for dedicated workers.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const dedicatedWorkerSession = await new Promise(async resolve => {
    const dedicatedWorkerSessionPromise = new Promise(resolve => {
      dp.Target.onAttachedToTarget(event => {
        resolve(session.createChild(event.params.sessionId));
      });
    });

    await session.evaluateAsync(`
        new Promise(resolve => {
          const worker = new Worker('resources/dedicated-worker.js')
          worker.onmessage = () => resolve();
        });`);

    resolve(await dedicatedWorkerSessionPromise);
  });
  const dwdp = dedicatedWorkerSession.protocol;

  await dwdp.Network.enable();

  // The network conditions of the worker session apply to the requests that
  // the dedicated worker makes. The conditions of the main target don't.
  await dwdp.Network.emulateNetworkConditionsByRule({
    matchedNetworkConditions: [{
      urlPattern: '',
      offline: true,
      latency: 0,
      downloadThroughput: -1,
      uploadThroughput: -1,
    }],
  });

  // Workers don't support emulateNetworkConditions, which overrides
  // navigator.onLine. Calling it on the main target overrides navigator.onLine
  // in the dedicated worker too.
  await dp.Network.emulateNetworkConditions({
    downloadThroughput: -1,
    latency: 0,
    offline: true,
    uploadThroughput: -1
  });

  dedicatedWorkerSession.evaluate(`fetch('/')`);
  const loadingFailed = await dwdp.Network.onceLoadingFailed();
  testRunner.log('loadingFailed.params.errorText: ' + loadingFailed.params.errorText);

  testRunner.log('navigator.onLine: ' + await dedicatedWorkerSession.evaluate('navigator.onLine'));

  testRunner.completeTest();
})

// TODO(jarhar): Add a test like this for shared workers when we can auto attach
// to them: http://crbug.com/851323
// https://chromium-review.googlesource.com/c/chromium/src/+/2198548
