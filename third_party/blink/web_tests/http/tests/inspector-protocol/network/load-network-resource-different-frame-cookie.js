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

  async function requestSourceMap(dp, frameId, testExplanation, url, options) {
    const response = await dp.Network.loadNetworkResource({frameId, url, options: {disableCache: false, ...options}});
    testRunner.log(response.result, testExplanation, ["headers", "stream"]);
    if (response.result.resource.success) {
      let result = await dp.IO.read({handle: response.result.resource.stream, size: 1000*1000});
      testRunner.log(`Steam content:`)
      testRunner.log(result.result.data);
      await dp.IO.close({handle: response.result.resource.stream});
    }
    testRunner.log(``);
  }

  for (const {frameId, dp} of frames.values()) {
    const url = `https://localhost:8443/inspector-protocol/network/resources/source.map.php`;
    await requestSourceMap(dp, frameId, `Response for fetch: `, url, {includeCredentials: true});
  }

  // Now test cookie behavior.
  const setCookieUrl = 'https://devtools.oopif-a.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
  + encodeURIComponent('name=value; SameSite=None; Secure');
  await session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);

  const setCookieUrl2 = 'https://devtools.oopif-a.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
  + encodeURIComponent('nameStrict=value2; SameSite=Strict; Secure');
  await session.evaluate(`fetch('${setCookieUrl2}', {method: 'POST', credentials: 'include'})`);

  const setCookieUrl3 = 'https://devtools.oopif-a.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
  + encodeURIComponent('nameOther=value3; SameSite=Lax; Secure');
  await session.evaluate(`fetch('${setCookieUrl3}', {method: 'POST', credentials: 'include'})`);

  for (const [frameUrl, {dp, frameId}] of frames.entries()) {
    const parsedURL = new URL(frameUrl);
    const url = `${parsedURL.protocol}//${parsedURL.host}/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_COOKIE`;
    for (const includeCredentials of [true, false]) {
      await requestSourceMap(dp, frameId, `Response for fetching\n${url}\n from\n${frameUrl}\nafter setting cookie ${includeCredentials?"including":"including only samesite"} credentials: `, url, {includeCredentials});
    }
  }

  testRunner.completeTest();
})
