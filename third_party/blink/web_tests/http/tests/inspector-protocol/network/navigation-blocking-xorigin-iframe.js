(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that navigations in cross-origin subframes are correctly blocked when intercepted.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let interceptionLog = [];
  async function onRequestIntercepted(dp, e) {
    const response = {interceptionId: e.params.interceptionId};

    if (e.params.request.url === 'http://devtools.oopif-b.test:8000/inspector-protocol/resources/test-page.html')
      response.errorReason = 'Aborted';
    interceptionLog.push(e.params.request.url + (response.errorReason ? `: ${response.errorReason}` : ''));

    await dp.Network.continueInterceptedRequest(response);
  }

  // Main frame load + initial oopif-a load + initial oopif-b load +
  // oopif-a redirect + oopif-b redirect.
  const expectedRequests = 5;
  let loadCount = 0;
  let loadCallback;
  const loadPromise = new Promise(fulfill => loadCallback = fulfill);

  const allTargets = [];
  async function initalizeTarget(dp) {
    allTargets.push(dp);
    await Promise.all([
      dp.Network.setRequestInterception({patterns: [{}]}),
      dp.Network.onRequestIntercepted(onRequestIntercepted.bind(this, dp)),
      dp.Network.enable(),
      dp.Page.enable()
    ]);
    dp.Page.onFrameStartedLoading(e => {
      ++loadCount;
    });
    dp.Page.onFrameStoppedLoading(e => {
      // When loading is done, we should have done 5 network requests. But (with
      // site isolation off) we may finish loading both iframes before the
      // redirects start, and loadCount would go to zero. So we want to wait for
      // loadCount to go to zero after we have done the redirects.
      if (!--loadCount && interceptionLog.length == expectedRequests)
        loadCallback();
    });
    await dp.Runtime.runIfWaitingForDebugger();
  }

  await initalizeTarget(dp);
  dp.Target.onAttachedToTarget(async e => {
    const targetProtocol = session.createChild(e.params.sessionId).protocol;
    await initalizeTarget(targetProtocol);
  });

  await dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/resources/iframe-navigation.html'});

  let urls = [];
  function getURLsRecursively(frameTree) {
    urls.push(frameTree.frame.url);
    (frameTree.childFrames || []).forEach(getURLsRecursively);
  }

  await loadPromise;
  let trees = await Promise.all(allTargets.map(target => target.Page.getFrameTree()));
  trees.map(result => result.result.frameTree).forEach(getURLsRecursively);

  testRunner.log('Interceptions:');
  testRunner.log(interceptionLog.sort());
  testRunner.log('Frames in page:');
  testRunner.log(urls.sort());

  testRunner.completeTest();
})
