(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Network.requestWillBeSent and other network events are emitted for worker main script and import scripts`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let expectedEvents = 9;

  const idToEvents = new Map();

  function onEvent() {
    --expectedEvents;
    if (expectedEvents)
      return;

    const items = [...idToEvents.values()].sort((a, b) => a.url.localeCompare(b.url));
    for (const { url, events } of items) {
      testRunner.log(url);
      testRunner.log(events.sort());
    }
    testRunner.completeTest();
  }

  function instrument(dp, prefix) {
    function logEvent(e) {
      idToEvents.get(e.params.requestId).events.push(prefix + e.method);
      onEvent();
    }

    dp.Network.onRequestWillBeSent(e => {
      idToEvents.set(e.params.requestId, {
        url: e.params.request.url,
        events: [prefix + e.method],
      });
      onEvent();
    });
    dp.Network.onResponseReceived(logEvent);
    dp.Network.onLoadingFinished(logEvent);
    dp.Network.onLoadingFailed(logEvent);
  }

  dp.Target.onAttachedToTarget(async event => {
    const wdp = session.createChild(event.params.sessionId).protocol;
    instrument(wdp, 'worker.');
    wdp.Network.enable();
    wdp.Runtime.runIfWaitingForDebugger();
  });

  instrument(dp, 'page.');
  await dp.Network.enable();
  await page.navigate(testRunner.url('./resources/worker-with-import.html'));
})
