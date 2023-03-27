(async function(testRunner) {
  const {session, dp, page} = await testRunner.startBlank(
      `Tests debugger disconnect while removing a regexp breakpoint.`);

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Debugger.enable();

  const name = '0'.repeat(20);
  for (let i = 0; i < 10; i++) {
    await session.evaluateAsync(`//# sourceURL=${name}${i}.js`);
  }

  const {result: {breakpointId}} = await dp.Debugger.setBreakpointByUrl(
      {lineNumber: 5, columnNumber: 0, urlRegex: '^(0+)*x$'});
  testRunner.log('Called setBreakpointByUrl (with regex)');

  // [crbug.com/1426163] Disconnecting the debugger while
  // in removeBreakpoint should not crash. Note that removeBreakpoint
  // with regexp can be interrupted in v8; let us try to disconnect
  // during that interrupt.
  dp.Debugger.removeBreakpoint({breakpointId});
  testRunner.log('Called removeBreakpoint (with regex)');

  await session.disconnect();
  testRunner.log('Disconnected');

  // Let's try to connect to the page to make sure it is still alive.
  const session2 = await page.createSession();
  await session2.protocol.Debugger.enable();

  testRunner.completeTest();
})
