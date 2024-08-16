(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Verify that the `Page.frameRequestedNavigation` event is emitted for the initial frame navigation.');

  const frameNameById = new Map();
  function onFrameNavigated(event) {
    const frame = event.params.frame;
    frameNameById.set(frame.id, frame.name ?? '<no name>');
  }
  dp.Page.onFrameNavigated(onFrameNavigated);
  await dp.Page.enable();

  dp.Target.onAttachedToTarget(async e => {
    const dp2 = session.createChild(e.params.sessionId).protocol;
    dp2.Page.onFrameNavigated(onFrameNavigated);
    await dp2.Page.enable();
    await dp2.Runtime.runIfWaitingForDebugger();
  });
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const frameRequestedNavigationEvents = [];
  dp.Page.onFrameRequestedNavigation((event) => {
    frameRequestedNavigationEvents.push(event);
  });

  dp.Page.navigate(
      {url: testRunner.url('resources/iframe-src-one-nested.html')});
  await dp.Page.onceLoadEventFired();

  for (const event of frameRequestedNavigationEvents) {
    testRunner.log(
      `Frame ${frameNameById.get(event.params.frameId) || '<no name>'} requested navigation to ${event.params.url}: ${event.params.reason}`);
  }

  testRunner.completeTest();
});
