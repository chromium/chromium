(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      `http://devtools.test:8000/inspector-protocol/debugger/resources/broken.html`,
      'Regression test for crbug.com/493631402');

  await dp.Debugger.enable();

  await dp.Debugger.setPauseOnExceptions({ state: 'all' });
  await dp.Page.reload();
  await dp.Debugger.onceScriptFailedToParse();

  // Run a longer running snippet that setBreakpointByUrl has to interrupt interrupt.
  const evalPromise = dp.Runtime.evaluate({ expression: "let sum = 0; for(let i=0; i<100_000; i++){sum+=i;}" });
  dp.Debugger.setBreakpointByUrl({
    lineNumber: 5,
    urlRegex: ".*"
  });

  dp.Debugger.disable();

  await evalPromise;
  testRunner.completeTest();
});
