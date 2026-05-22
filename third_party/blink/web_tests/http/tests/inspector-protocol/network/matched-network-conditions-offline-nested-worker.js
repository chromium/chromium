(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Verify pattern matched network conditions offline emulation are applied to worker main script in a worker.`);

  async function setConditions(conditions, targetDp) {
    const matchedNetworkConditions = conditions.map(condition => ({
                                                      latency: 0,
                                                      downloadThroughput: -1,
                                                      uploadThroughput: -1,
                                                      ...condition
                                                    }));
    const {result: {ruleIds}} =
        await targetDp.Network.emulateNetworkConditionsByRule({
          matchedNetworkConditions,
        });
    return ruleIds;
  }

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  await dp.Target.setAutoAttach({
    autoAttach: true,
    waitForDebuggerOnStart: true,
    flatten: true,
  });

  dp.Target.onAttachedToTarget(async event => {
    const wdp = session.createChild(event.params.sessionId).protocol;
    await wdp.Network.enable();
    await wdp.Runtime.enable();
    await wdp.Network.setCacheDisabled({cacheDisabled: true});

    // Block the specific worker script pattern
    await setConditions(
        [{
          urlPattern: 'http://*:*/*offline-worker.js*',
          offline: true,
        }],
        wdp);

    await wdp.Runtime.runIfWaitingForDebugger();
  });

  const result = await session.evaluateAsync(
      (parentWorkerUrl, workerUrl) => {
        return new Promise(resolve => {
          const worker = new Worker(`${parentWorkerUrl}`);
          worker.onmessage = (e) => {
            resolve(e.data);
          };
          worker.onerror = (e) => {
            e.preventDefault();
            resolve('parent worker failed to load');
          };
          worker.postMessage(`${workerUrl}`);
        });
      },
      testRunner.url('resources/parent-worker.js'),
      testRunner.url('resources/offline-worker.js'));

  testRunner.log('nested worker script load result: ' + result);

  testRunner.completeTest();
})
