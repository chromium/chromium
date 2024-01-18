(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that fenced frame target does not support ' +
      'Emulation.setEmitTouchEventsForMouse');

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  const ffdp = session.createChild(sessionId).protocol;

  const result = await ffdp.Emulation.setEmitTouchEventsForMouse({enabled: true});

  testRunner.log(result.error ? 'FAIL: ' + result.error.message : 'PASS');
  testRunner.completeTest();
})
