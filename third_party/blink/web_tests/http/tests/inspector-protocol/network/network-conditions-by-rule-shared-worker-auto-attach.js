(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Verify that Network.emulateNetworkConditionsByRule sent right after a ' +
      'shared worker is auto-attached (before the worker is resumed) applies ' +
      'to requests made by the worker.');

  const bp = testRunner.browserP();
  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.evaluate(`
    window.worker = new SharedWorker(
        '/inspector-protocol/network/resources/shared-worker-fetch.js');
    window.worker.port.start();
    window.workerMessage = () => new Promise(resolve => {
      window.worker.port.onmessage = event => resolve(event.data);
    });
    window.fetchInWorker = url => {
      const result = window.workerMessage();
      window.worker.port.postMessage(url);
      return result;
    };
    undefined;
  `);

  const attached = await bp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'shared_worker');
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

  const matching = await session.evaluateAsync(
      `window.fetchInWorker('/inspector-protocol/network/resources/hello-world.html')`);
  testRunner.log(`Matching request: ${matching}`);

  const nonMatching = await session.evaluateAsync(
      `window.fetchInWorker('/inspector-protocol/network/resources/worker.js')`);
  testRunner.log(`Non-matching request: ${nonMatching}`);

  testRunner.completeTest();
});
