(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests cross site navigation during instrumentation pause.`);

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Page.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});

  const result = session.evaluate('42');

  const expressionPause = await dp.Debugger.oncePaused();
  testRunner.log(`paused reason: ${expressionPause.params.reason}`);

  const navigated = dp.Page.onceFrameNavigated();
  session.navigate('http://devtools.oopif.test:8000/inspector-protocol/');

  dp.Debugger.resume({terminateOnresume: false});
  await dp.Debugger.onceResumed();
  testRunner.log(`resumed`);

  testRunner.log(`navigated ${(await navigated).params.frame.url}`);

  testRunner.completeTest();
})
