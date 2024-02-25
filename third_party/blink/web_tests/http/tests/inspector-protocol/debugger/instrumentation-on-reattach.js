(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that disconnecting devtools from the page in an instrumentation pause will resume the render process.`);

  await dp.Target.setDiscoverTargets({discover: true});
  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened a second window');

  const event = await dp.Target.onceTargetInfoChanged(
      event =>
          event.params.targetInfo.url.endsWith('inspector-protocol-page.html'));
  const targetId = event.params.targetInfo.targetId;

  {
    const sessionId =
        (await dp.Target.attachToTarget({targetId, flatten: true}))
            .result.sessionId;
    const childSession = session.createChild(sessionId);
    testRunner.log('Attached to the second window');
    await childSession.protocol.Debugger.enable();
    await childSession.protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});

    // Evaluate a script, this triggers an instrumentation pause.
    childSession.evaluate('1');

    const pause = await childSession.protocol.Debugger.oncePaused();
    testRunner.log(
        'Paused in the second window; reason: ' + pause.params.reason);

    await childSession.disconnect();
    testRunner.log('Disconnected from the child session');
  }

  {
    const sessionId =
        (await dp.Target.attachToTarget({targetId, flatten: true}))
            .result.sessionId;
    const childSession = session.createChild(sessionId);
    testRunner.log('Re-attached to the second window');
    await childSession.protocol.Debugger.enable();
    await childSession.protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});

    childSession.evaluate('2');

    const pause = await childSession.protocol.Debugger.oncePaused();
    testRunner.log('Paused in a second window; reason: ' + pause.params.reason);
    await childSession.protocol.Debugger.resume();
  }

  session.protocol.Target.closeTarget({targetId});
  testRunner.log('Closed the second window');
  await dp.Target.onceTargetDestroyed(
      event => event.params.targetId === targetId);
  testRunner.log('Received window destroyed notification');
  testRunner.completeTest();
})
