(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Verify pattern matched network conditions offline emulation are applied to worker main script in the main frame.`);

  async function setConditions(conditions) {
    const matchedNetworkConditions = conditions.map(condition => ({
                                                      latency: 0,
                                                      downloadThroughput: -1,
                                                      uploadThroughput: -1,
                                                      ...condition
                                                    }));
    const {result: {ruleIds}} =
        await dp.Network.emulateNetworkConditionsByRule({
          matchedNetworkConditions,
        });
    return ruleIds;
  }

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  // Block the specific worker script pattern
  await setConditions([{
    urlPattern: 'http://*:*/*offline-worker.js*',
    offline: true,
  }]);

  const result = await session.evaluateAsync((workerUrl) => {
    return new Promise(resolve => {
      const worker = new Worker(`${workerUrl}`);
      worker.onmessage = () => {
        resolve('worker loaded successfully');
      };
      worker.onerror = (e) => {
        e.preventDefault();
        resolve('worker failed to load');
      };
    });
  }, testRunner.url('resources/offline-worker.js'));

  testRunner.log('main frame worker script load result: ' + result);

  testRunner.completeTest();
})
