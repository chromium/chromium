(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests setting a breakpoint during instrumentation pause.`);

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Debugger.enable();
  await dp.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});

  const result = session.evaluate(`
    function testFunction(x) {
      return x;
    }
    testFunction();
    //# sourceURL=test.js`);
  const expressionPause = await dp.Debugger.oncePaused();
  testRunner.log(`paused (1) reason: ${expressionPause.params.reason}`);

  const bp = await dp.Debugger.setBreakpointByUrl(
      {lineNumber: 3, columnNumber: 0, url: 'test.js'});
  const locations =
      bp.result.locations.map(l => `${l.lineNumber}:${l.columnNumber}`).join();
  testRunner.log(`breakpoint locations: ${locations}`);

  const scriptId = bp.result.locations[0].scriptId;

  const sourceResponse = await dp.Debugger.getScriptSource({scriptId});
  testRunner.log(sourceResponse.result.scriptSource);

  dp.Debugger.resume({terminateOnresume: false});
  await dp.Debugger.onceResumed();
  testRunner.log(`resumed (1)`);

  // Verify we stop on the breakpoint we placed in the instrumentation pause.
  const breakpointPause = await dp.Debugger.oncePaused();
  testRunner.log(`paused (2) reason: ${breakpointPause.params.reason}`);
  const pausedLocation = breakpointPause.params.callFrames[0].location;
  testRunner.log(`paused (2) location: ${pausedLocation.lineNumber}:${
      pausedLocation.columnNumber}`);
  const hitBreakpoints = breakpointPause.params.hitBreakpoints;
  testRunner.log(`paused (2) hitBreakpoints count ${hitBreakpoints.length}`);
  testRunner.log(`paused (2) hitBreakpoint[0] matches: ${
      hitBreakpoints[0] === bp.result.breakpointId}`);

  testRunner.completeTest();
})
