(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Verify pattern matched network conditions offline emulation are applied to frames.`);

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

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
    await dp2.Network.enable();
    await dp2.Runtime.enable();
    await dp2.Page.enable();
    dp2.Page.onFrameNavigated(checkFrameNavigated);

    const matchedNetworkConditions = [{
      urlPattern: '*://*:*/*frames/frame.html',
      latency: 0,
      downloadThroughput: -1,
      uploadThroughput: -1,
      offline: true,
    }];
    await dp2.Network.emulateNetworkConditionsByRule({
      matchedNetworkConditions,
    });

    await dp2.Runtime.runIfWaitingForDebugger();
  });

  await dp.Page.enable();
  const loadPromise = dp.Page.onceLoadEventFired();
  await session.navigate('resources/page-with-iframe.html');
  await loadPromise;

  const matchedNetworkConditions = [{
    urlPattern: '*://*:*/*frames/frame.html',
    latency: 0,
    downloadThroughput: -1,
    uploadThroughput: -1,
    offline: true,
  }];
  await dp.Network.emulateNetworkConditionsByRule({
    matchedNetworkConditions,
  });

  await session.evaluate((url) => {
    document.getElementById('iframe').src = url;
  }, 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/frames/frame.html');

  const result = await navigated;
  testRunner.log('frame.html:' + result);

  testRunner.completeTest();
})
