(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('../resources/empty.html',
    'Tests that fenced frame automatic beacons are surfaced to dev tools.');

  dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  // Create a selectURL fenced frame with a reporting beacon registered.
  session.evaluate(async function () {
    const href = new URL('../fenced-frame/resources/page-with-title.php',
        location.href);
    await sharedStorage.worklet.addModule(
      "../fenced-frame/resources/simple-shared-storage-module.js");
    const config = await sharedStorage.selectURL(
        'test-url-selection-operation', [{
          url: href,
          reportingMetadata: {
            'reserved.top_navigation':
                '../fenced-frame/resources/beacon-store.py'
          }
        }],
        {
          data: {'mockResult': 0},
          resolveToConfig: true,
        });
    let ff = document.createElement('fencedframe');
    ff.config = config;
    document.body.appendChild(ff);
  });

  const {sessionId: workletSessionId} = (await dp.Target.onceAttachedToTarget()).params;
  const workletSession = session.createChild(workletSessionId);
  const workletdp = workletSession.protocol;
  workletdp.Runtime.runIfWaitingForDebugger();

  const {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  const ffSession = session.createChild(sessionId);
  const ffdp = ffSession.protocol;

  ffdp.Page.enable();
  ffdp.Runtime.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});

  // Set up promises for the network events we'll need.
  ffdp.Network.enable();
  ffdp.Runtime.runIfWaitingForDebugger();

  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  const requestWillBeSentPromise = ffdp.Network.onceRequestWillBeSent();
  const requestWillBeSentExtraInfoPromise =
      ffdp.Network.onceRequestWillBeSentExtraInfo();
  const responseReceivedPromise = ffdp.Network.onceResponseReceived();
  const loadingFinishedPromise = ffdp.Network.onceLoadingFinished();

  // Trigger the network request with reportEvent.
  await ffSession.evaluateAsyncWithUserGesture(function() {
    window.fence.setReportEventDataForAutomaticBeacons({
      eventType: 'reserved.top_navigation',
      eventData: 'This is the event data!',
      destination: ['shared-storage-select-url']
    });
    window.open("page-with-title.php", "_blank");
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
