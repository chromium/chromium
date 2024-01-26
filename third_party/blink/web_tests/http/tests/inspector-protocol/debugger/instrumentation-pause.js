(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests instrumentation pause delays evaluations.`);

  // This test checks that side effects, such as (nested) evaluations, are
  // delayed until after the instrumentation pause.

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Debugger.enable();
  await dp.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});

  const outerResult = session.evaluate(`"Expression " + 1`);

  const outerExpressionPause = await dp.Debugger.oncePaused();
  testRunner.log(`paused (1) reason: ${outerExpressionPause.params.reason}`);

  // Nested evaluation will be handled after the instrumentation breakpoint
  // resumes.
  const nestedResult = session.evaluate(`"Expression " + 2`);

  dp.Debugger.resume({terminateOnresume: false});
  await dp.Debugger.onceResumed();
  testRunner.log(`resumed (1)`);

  testRunner.log(`First evaluation result: ${await outerResult}`);

  // The nested evaluation also causes instrumentation pause.
  const nestedExpressionPause = await dp.Debugger.oncePaused();
  testRunner.log(`paused (2) reason: ${nestedExpressionPause.params.reason}`);

  dp.Debugger.resume({terminateOnresume: false});
  await dp.Debugger.onceResumed();
  testRunner.log(`resumed (2)`);

  testRunner.log(`Second evaluation result: ${await nestedResult}`);

  testRunner.completeTest();
})
