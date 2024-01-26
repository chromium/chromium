(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests Page.loadNetworkResource for different frames with cookies`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});


  let loadCount = 5;
  let loadCallback;
  const loadPromise = new Promise(fulfill => loadCallback = fulfill);

  const allTargets = [];
  async function initalizeTarget(dp) {
    allTargets.push(dp);
    await dp.Page.enable();
    await dp.Network.enable();
    dp.Page.onFrameStoppedLoading(e => {
      if (!--loadCount)
        loadCallback();
    });
    await dp.Runtime.runIfWaitingForDebugger();
  }

  dp.Target.onAttachedToTarget(async e => {
    const child = session.createChild(e.params.sessionId);
    const targetProtocol = child.protocol;
    await initalizeTarget(targetProtocol);
  });
  await initalizeTarget(dp);

  await dp.Page.navigate({url: 'https://127.0.0.1:8443/inspector-protocol/resources/iframe-navigation-secure.html'});

  const frames = new Map();
  function getFrameIds(dp, frameTree) {
    frames.set(frameTree.frame.securityOrigin, {frameId: frameTree.frame.id, dp});
    (frameTree.childFrames || []).forEach(getFrameIds.bind(null, dp));
  }

  await loadPromise;
  const frameTargetList = [];
  for (const dp of allTargets) {
    const {result} = await dp.Page.getFrameTree();
    frameTargetList.push({dp, frameTree: result.frameTree});
  }
  frameTargetList.sort((a,b) => a.frameTree.frame.url.localeCompare(b.frameTree.frame.url));
  frameTargetList.forEach(({dp, frameTree}) => getFrameIds(dp, frameTree));

  testRunner.log(`Number of frames in page: ${frames.size}`);

  // Try to fetch resources on behalf of OOPIFs to provocate an error.
  for (const {frameId} of frames.values()) {
    const url = `https://localhost:8443/inspector-protocol/network/resources/source.map`;
    const response1 = await dp.Network.loadNetworkResource({frameId, url, options: {disableCache:false, includeCredentials: false}});
    testRunner.log(response1, `Response for fetch: `, ["headers", "id", "sessionId", "stream"]);
  }

  testRunner.completeTest();
})
