(async function (testRunner) {
  const { session, dp } = await testRunner.startURL('../resources/empty.html',
    'Tests that fenced frame reporting beacons are surfaced to dev tools.');

  // Create a selectURL fenced frame with a reporting beacon registered.
  await session.evaluateAsync(async function () {
    const href = new URL('../fenced-frame/resources/page-with-title.php', location.href);
    await sharedStorage.worklet.addModule(
      "../fenced-frame/resources/simple-shared-storage-module.js");
    const config = await sharedStorage.selectURL(
        'test-url-selection-operation', [{url: href,
            reportingMetadata: {'click': '../fenced-frame/resources/automatic-beacon-store.py'}}], {
          data: {'mockResult': 0},
          resolveToConfig: true,
        });
    let ff = document.createElement('fencedframe');
    ff.config = config;
    document.body.appendChild(ff);
  });

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  let ffSession = session.createChild(sessionId);
  let ffdp = ffSession.protocol;

  ffdp.Page.enable();
  ffdp.Runtime.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  // Set up promises for the network events we'll need.
  ffdp.Network.enable();
  const requestWillBeSentPromise = ffdp.Network.onceRequestWillBeSent();
  const requestWillBeSentExtraInfoPromise = ffdp.Network.onceRequestWillBeSentExtraInfo();
  const responseReceivedPromise = ffdp.Network.onceResponseReceived();
  const loadingFinishedPromise = ffdp.Network.onceLoadingFinished();
  ffdp.Runtime.runIfWaitingForDebugger();

  // Trigger the network request with reportEvent.
  await ffSession.evaluate(function() {
    fence.reportEvent({eventType: 'click', eventData: 'dummy',
      destination: ['shared-storage-select-url']});
  });

  const [request, requestExtraInfo, response, loading] = await Promise.all([
      requestWillBeSentPromise,
      requestWillBeSentExtraInfoPromise,
      responseReceivedPromise,
      loadingFinishedPromise,
  ]);

  // The initial request should have no headers.
  testRunner.log('request url: ' + request.params.documentURL);
  testRunner.log('request headers: ' + request.params.headers);

  // Then the requestExtraInfo specifies the headers.
  testRunner.log('requestExtraInfo has same requestId: '
      + (request.requestId === requestExtraInfo.requestId));
  testRunner.log('requestExtraInfo has headers: '
      + (Object.keys(requestExtraInfo.params.headers).length > 0));

  // The request should succeed with a 200 status code.
  testRunner.log('responseReceived has same requestId: '
      + (request.requestId === response.requestId));
  testRunner.log('responseReceived status: '
      + response.params.response.status);
  testRunner.log('loadingFinished has same requestId: '
      + (request.requestId === loading.requestId));

  testRunner.completeTest();
});
