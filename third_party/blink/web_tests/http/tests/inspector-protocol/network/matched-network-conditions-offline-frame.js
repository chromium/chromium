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

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let resolveNavigated;
  const navigated = new Promise(resolve => {
    resolveNavigated = resolve;
  });

  const checkFrameNavigated = event => {
    const frame = event.params.frame;
    if (frame.url.startsWith('chrome-error://')) {
      resolveNavigated('failed to load');
    } else if (frame.url.includes('frames/frame.html')) {
      resolveNavigated('loaded');
    }
  };

  dp.Page.onFrameNavigated(checkFrameNavigated);

  dp.Target.onAttachedToTarget(async event => {
    const dp2 = session.createChild(event.params.sessionId).protocol;
    await dp2.Page.enable();
    dp2.Page.onFrameNavigated(checkFrameNavigated);
    await dp2.Runtime.runIfWaitingForDebugger();
  });

  await session.evaluate((url) => {
    document.getElementById('iframe').src = url;
  }, testRunner.url('resources/frames/frame.html'));

  const result = await navigated;
  testRunner.log('frame.html:' + result);

  testRunner.completeTest();
})
