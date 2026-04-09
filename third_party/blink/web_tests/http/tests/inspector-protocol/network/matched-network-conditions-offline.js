(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Verify pattern matched network conditions offline emulation are applied.`);

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

  async function request(resource) {
    return await session.evaluateAsync((url) => {
      return fetch(url).then(r => r.status).catch(error => error.message);
    }, testRunner.url(resource));
  }

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  await setConditions([
    {
      urlPattern: 'http://*:*/*/a.html',
      offline: true,
    },
    {
      urlPattern: 'http://*:*/*/b.html',
      offline: false,
    }
  ]);
  testRunner.log('a.html:' + await request('resources/a.html'));
  testRunner.log('b.html:' + await request('resources/b.html'));

  testRunner.completeTest();
})
