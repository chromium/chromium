(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Verify that the network conditions of the worker session, and not ' +
      'those of the page session, apply to the requests of a dedicated worker.');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const attached = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'worker');
  await session.evaluateAsync(`
    new Promise(resolve => {
      window.worker = new Worker(
          '/inspector-protocol/network/resources/dedicated-worker.js');
      window.worker.onmessage = () => resolve();
    })`);
  const workerSession = session.createChild((await attached).params.sessionId);
  const workerDp = workerSession.protocol;

  const rule = {
    matchedNetworkConditions: [{
      urlPattern: 'http://*:*/*hello-world.html*',
      offline: true,
      latency: 0,
      downloadThroughput: -1,
      uploadThroughput: -1,
    }],
  };
  const fetchExpression = `
    fetch('/inspector-protocol/network/resources/hello-world.html',
          {cache: 'no-store'})
        .then(() => 'succeeded', () => 'failed')`;

  await dp.Network.enable();
  await dp.Network.emulateNetworkConditionsByRule(rule);
  testRunner.log(`Page request, rule on the page session: ${
      await session.evaluateAsync(fetchExpression)}`);
  testRunner.log(`Worker request, rule on the page session: ${
      await workerSession.evaluateAsync(fetchExpression)}`);

  // Dedicated workers don't support Network.emulateNetworkConditions. The
  // command must have no effect when it fails.
  const response = await workerDp.Network.emulateNetworkConditions({
    offline: true,
    latency: 0,
    downloadThroughput: -1,
    uploadThroughput: -1
  });
  testRunner.log(`emulateNetworkConditions on the worker session: ${
      response.error ? response.error.message : 'succeeded'}`);
  testRunner.log(`Worker request, after emulateNetworkConditions: ${
      await workerSession.evaluateAsync(fetchExpression)}`);

  await workerDp.Network.emulateNetworkConditionsByRule(rule);
  testRunner.log(`Worker request, rule on the worker session: ${
      await workerSession.evaluateAsync(fetchExpression)}`);

  testRunner.completeTest();
});
