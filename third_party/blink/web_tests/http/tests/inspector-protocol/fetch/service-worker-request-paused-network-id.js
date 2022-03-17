(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that networkId present on Fetch.requestPaused for initial Service Worker script request`);

  const assertNetworkIdAlignment = (description, willBeSentEvent, pausedEvent) => {
    const {params: {requestId}} = willBeSentEvent;
    const {params: {networkId}} = pausedEvent;
    if (networkId && networkId === requestId) {
      testRunner.log(`OK: ${description}: networkId === requestId`)
      return;
    }

    testRunner.fail(`FAIL: ${
        description}: networkId !== requestId or one or more ids are missing (${
        networkId} vs. ${requestId})`);
  };

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let workerProtocol = new Promise(resolve => {
    dp.Target.onAttachedToTarget(async event => {
      const wdp = session.createChild(event.params.sessionId).protocol;
      resolve(wdp);

      wdp.Fetch.onRequestPaused(e => {
        wdp.Fetch.continueRequest({
          requestId: e.params.requestId,
        });
      });
      wdp.Network.enable();
      wdp.Fetch.enable({patterns: [{urlPattern: '*'}]});
      wdp.Runtime.runIfWaitingForDebugger();
    });
  });

  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{urlPattern: '*'}]});

  dp.Fetch.onRequestPaused(event => {
    dp.Fetch.continueRequest({
      requestId: event.params.requestId,
    });
  });

  const [[
    initScriptWillBeSentEvent, initScriptPausedEvent, subFetchWillBeSentEvent,
    subFetchPausedEvent
  ]] =
      await Promise.all([
        workerProtocol.then(wdp => Promise.all([
          wdp.Network.onceRequestWillBeSent(
              e => e.params.request.url.endsWith(
                  '/service-worker-with-fetch.js')),
          wdp.Fetch.onceRequestPaused(
              e => e.params.request.url.endsWith(
                  '/service-worker-with-fetch.js')),
          wdp.Network.onceRequestWillBeSent(
              e => e.params.request.url.endsWith(
                  '/request-within-service-worker')),
          wdp.Fetch.onceRequestPaused(
              e => e.params.request.url.endsWith(
                  '/request-within-service-worker')),
        ])),
        page.navigate(
            testRunner.url('./resources/service-worker-with-fetch.html')),
      ]);

  assertNetworkIdAlignment(
      'init', initScriptWillBeSentEvent, initScriptPausedEvent);
  assertNetworkIdAlignment(
      'subFetch', subFetchWillBeSentEvent, subFetchPausedEvent);

  testRunner.completeTest();
})
