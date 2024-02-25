(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that closing the parent page will detach a sub-frame in instrumentation pause.`);

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Page.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const frameAttached = dp.Target.onceAttachedToTarget();

  session.evaluate(`
    const frame = document.createElement('iframe');
    frame.src = 'http://devtools.oopif.test:8080/inspector-protocol/resources/iframe-with-script.html';
    document.body.appendChild(frame);`);

  const {sessionId, targetInfo: {targetId: frameTargetId}} =
      (await frameAttached).params;
  const {protocol: frameProtocol} = session.createChild(sessionId);

  frameProtocol.Target.onAttachedToTarget(
      event =>
          testRunner.log(`Attached to target ${event.params.targetInfo.url}`));

  await frameProtocol.Debugger.enable();
  await frameProtocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});
  await frameProtocol.Runtime.runIfWaitingForDebugger();

  const pause = await frameProtocol.Debugger.oncePaused();
  testRunner.log('Paused in the sub-frame; reason: ' + pause.params.reason);

  const detach = dp.Target.onceDetachedFromTarget(
      event => event.params.targetId === frameTargetId);

  const navigated = dp.Page.onceFrameNavigated();
  await session.navigate(
      'http://localhost:8000/inspector-protocol/debugger/resources/empty.html');

  testRunner.log(`Page navigated to ${(await navigated).params.frame.url}`);

  await detach;
  testRunner.log('Frame detached');

  testRunner.completeTest();
})
