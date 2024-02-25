(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('../resources/empty.html',
    'Tests that network events for a newly created fenced frame can be observed after auto-attach.');

  dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: true, flatten: true });
  session.evaluate(function() {
    let ff = document.createElement('fencedframe');
    const url = new URL('../fenced-frame/resources/page-with-title.php',
        location.href);
    ff.config = new FencedFrameConfig(url);
    document.body.appendChild(ff);
  });

  let { targetInfo, sessionId } = (await dp.Target.onceAttachedToTarget()).params;
  let ff_session = session.createChild(sessionId);
  let ff_dp = ff_session.protocol;
  ff_dp.Network.enable();
  ff_dp.Runtime.runIfWaitingForDebugger();

  const requestWillBeSentPromise = ff_dp.Network.onceRequestWillBeSent();
  const responseReceivedPromise = ff_dp.Network.onceResponseReceived();
  const loadingFinishedPromise = ff_dp.Network.onceLoadingFinished();

  const requestWillBeSentParams = (await requestWillBeSentPromise).params;
  let requestId = undefined;
  if (requestWillBeSentParams.frameId === targetInfo.targetId) {
    testRunner.log('FF navigation request sent for url: ' + requestWillBeSentParams.documentURL);
    requestId = requestWillBeSentParams.requestId;
  }

  testRunner.log('FF navigation request isSameSite: ' + requestWillBeSentParams.request.isSameSite);

  const responseReceivedParams = (await responseReceivedPromise).params;
  if (responseReceivedParams.requestId == requestId) {
    testRunner.log('Response received for FF navigation request: ' + responseReceivedParams.response.status);
  }

  const loadingFinishedParams = (await loadingFinishedPromise).params;
  if (loadingFinishedParams.requestId == requestId) {
    testRunner.log('Loading finished for FF.');
  }

  testRunner.completeTest();
});
