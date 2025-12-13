(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests reloading during instrumentation pause.`);

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Page.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});

  const result = dp.Runtime.evaluate({expression: '42'});

  const expressionPause = await dp.Debugger.oncePaused();
  testRunner.log(`paused reason: ${expressionPause.params.reason}`);

  const navigated = dp.Page.onceFrameNavigated();
  const reloadPromise = dp.Page.reload();

  // Note that we won't actually wait for the resume, since it might not
  // actually resume anything if the reload finished before the resume
  // message is received.
  dp.Debugger.resume({terminateOnresume: false});
  testRunner.log(`resumed`);

  await reloadPromise;
  testRunner.log(`reloaded`);

  testRunner.log(`frame navigated ${(await navigated).params.frame.url}`);

  testRunner.completeTest();
})
