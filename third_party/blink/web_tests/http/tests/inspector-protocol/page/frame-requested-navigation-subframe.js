(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that when a frame casues another frame to navigate, frameId is that of target frame');

  await dp.Page.enable();

  const frameNameById = new Map();
  function onFrameNavigated(event) {
    const frame = event.params.frame;
    frameNameById.set(frame.id, frame.name || '<no name>');
  }
  dp.Page.onFrameNavigated(onFrameNavigated);

  dp.Target.onAttachedToTarget(async e => {
    const dp2 = session.createChild(e.params.sessionId).protocol;
    await dp2.Page.enable();
    dp2.Page.onFrameNavigated(onFrameNavigated);
    await dp2.Runtime.runIfWaitingForDebugger();
  });

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  await session.navigate('resources/frame-requested-navigation-subframe.html');

  async function waitForRequestedNavigationAndDump() {
    const frameRequestedNavigation = (await dp.Page.onceFrameRequestedNavigation()).params;
    const frameName = frameNameById.get(frameRequestedNavigation.frameId) || '<unknown frame>';
    const reason = frameRequestedNavigation.reason;
    testRunner.log(`Got frameRequestedNavigation from "${frameName}", reason: ${reason}`);
  }

  session.evaluate('document.forms[0].submit()');
  await waitForRequestedNavigationAndDump();

  session.evaluate('document.forms[1].submit()');
  await waitForRequestedNavigationAndDump();

  testRunner.completeTest();
})
