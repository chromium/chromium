(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that closing the page will resume the render process.`);

  await dp.Target.setDiscoverTargets({discover: true});
  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened a second window');

  const event = await dp.Target.onceTargetInfoChanged(
      event =>
          event.params.targetInfo.url.endsWith('inspector-protocol-page.html'));
  const targetId = event.params.targetInfo.targetId;

  const sessionId =
      (await dp.Target.attachToTarget({targetId: targetId, flatten: true}))
          .result.sessionId;
  const childSession = session.createChild(sessionId);
  testRunner.log('Attached to the second window');
  await childSession.protocol.Debugger.enable();
  await childSession.protocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});

  childSession.evaluate('1');
  const pause = await childSession.protocol.Debugger.oncePaused();
  testRunner.log('Paused in the second window; reason: ' + pause.params.reason);

  session.protocol.Target.closeTarget({targetId: targetId});
  testRunner.log('Scheduled closing the second window');

  childSession.protocol.Debugger.resume();
  testRunner.log('Resumed the instrumentation pause');

  await dp.Target.onceTargetDestroyed(
      event => event.params.targetId === targetId);
  testRunner.log('Received the target-destroyed event');

  testRunner.completeTest();
})
