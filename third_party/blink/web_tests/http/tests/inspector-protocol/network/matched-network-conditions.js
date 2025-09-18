(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Verify pattern matched network conditions are applied.`);
  /**
   * @param {Array<{urlPattern: String, offline: boolean}>} conditions
   * @return {Promise<string[]>}
   */
  async function setConditions(conditions) {
    const matchedNetworkConditions = conditions.map(condition => ({
                                                      offline: false,
                                                      latency: 0,
                                                      downloadThroughput: 0,
                                                      uploadThroughput: 0,
                                                      ...condition
                                                    }));
    const {result: {ruleIds}} =
        await dp.Network.emulateNetworkConditionsByRule({
          offline: false,
          matchedNetworkConditions,
        });
    return ruleIds;
  }

  /**
   * @param {string} url
   * @param {string[]} ruleIds
   * @return Promise<{url: string, affectingNetworkConditionsId?: number}>
   */
  async function request(resource, ruleIds) {
    const requestPromise = dp.Network.onceRequestWillBeSent();
    const extraInfoPromise = dp.Network.onceRequestWillBeSentExtraInfo();

    dp.Runtime.evaluate({
      expression: `
      fetch("${testRunner.url(resource)}").then(r => r.text())`
    });

    const [request, extraInfo] =
        await Promise.all([requestPromise, extraInfoPromise]);
    return {
      url: request.params.url,
      appliedNetworkConditionsId:
          ruleIds.indexOf(extraInfo.params.appliedNetworkConditionsId)
    };
  }

  await dp.Network.enable();
  await dp.Runtime.enable();

  await dp.Network.setCacheDisabled({cacheDisabled: true});

  const ruleIds = [];
  testRunner.log('Applies condtitions only to matching requests:');
  ruleIds.push(...await setConditions([
    {
      urlPattern: 'http://*:*/*/a.html',
      latency: 0.5,
    },
    {
      urlPattern: 'http://*:*/*/b.html',
      latency: 0.3,
    }
  ]));
  testRunner.log(await request('resources/a.html', ruleIds));
  testRunner.log(await request('resources/b.html', ruleIds));
  testRunner.log(await request('resources/b.html', ruleIds));
  testRunner.log(await request('resources/a.html', ruleIds));
  testRunner.log(await request('resources/a.html?foobar', ruleIds));

  testRunner.log('Applies global condtitions when no pattern is matching:');
  ruleIds.push(...await setConditions([{
    urlPattern: '',
    latency: 0.5,
  }]));
  testRunner.log(await request('resources/a.html', ruleIds));
  testRunner.log(await request('resources/b.html', ruleIds));

  testRunner.log('Ignores invalid patterns:');
  ruleIds.push(...await setConditions([{
    urlPattern: 'ht tp://*:*/*/a.html',
    latency: 0.5,
  }]));
  testRunner.log(await request('resources/a.html', ruleIds));
  testRunner.log(await request('resources/b.html', ruleIds));

  testRunner.completeTest();
})
