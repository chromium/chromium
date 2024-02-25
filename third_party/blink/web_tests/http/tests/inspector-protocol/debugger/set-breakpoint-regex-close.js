(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests setting a regexp breakpoint and disconnect.`);

  await dp.Target.setDiscoverTargets({discover: true});
  await dp.Debugger.enable();

  const name = '0'.repeat(25);
  for (let i = 0; i < 10; i++) {
    await session.evaluateAsync(`//# sourceURL=${name}${i}.js`);
  }

  // [crbug.com/1422830] Disconnecting the debugger while
  // in setBreakpointByUrl should not crash. Note that setBreakpointUrl
  // with regexp can be interrupted in v8; let us try to disconnect
  // during that interrupt.
  dp.Debugger.setBreakpointByUrl(
      {lineNumber: 5, columnNumber: 0, urlRegex: '^(0+)*x$'});
  testRunner.log('Called setBreakpointByUrl (with regex)');

  session.disconnect();
  testRunner.log('Disconnected');

  testRunner.completeTest();
})
