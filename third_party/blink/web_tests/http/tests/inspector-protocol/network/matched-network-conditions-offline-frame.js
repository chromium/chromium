(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Verify pattern matched network conditions offline emulation are applied to frames.`);

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

  await setConditions([{
    urlPattern: '*://*:*/*frames/frame.html',
    offline: true,
  }]);

  await dp.Page.enable();
  const loadPromise = dp.Page.onceLoadEventFired();
  await session.navigate('resources/page-with-iframe.html');
  await loadPromise;

  const navigated = new Promise(resolve => {
    dp.Page.onFrameNavigated(event => {
      const frame = event.params.frame;
      if (frame.parentId) {
        if (frame.url.startsWith('chrome-error://')) {
          resolve('failed to load');
        } else if (frame.url.includes('frames/frame.html')) {
          resolve('loaded');
        }
      }
    });
  });

  await session.evaluate((url) => {
    document.getElementById('iframe').src = url;
  }, testRunner.url('resources/frames/frame.html'));

  const result = await navigated;
  testRunner.log('frame.html:' + result);

  testRunner.completeTest();
})
