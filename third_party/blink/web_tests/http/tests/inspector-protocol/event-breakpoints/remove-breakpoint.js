(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that CanvasCreated breakpoing is hit when offscreen context is created in the page');

  dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(e => testRunner.log(`PAGE: ${e.params.args[0].value}`));

  dp.Debugger.enable();
  dp.Debugger.onPaused(async e => {
    testRunner.log(e.params.data, 'Paused: ');
    await dp.EventBreakpoints.removeInstrumentationBreakpoint({eventName: "canvasContextCreated"});
    dp.Debugger.resume();
  });

  dp.EventBreakpoints.setInstrumentationBreakpoint({eventName: "canvasContextCreated"});
  await session.evaluate(`
    const offscreen1 = new OffscreenCanvas(6, 7);
    const gl1 = offscreen1.getContext("webgl");
    console.log('created first context (should pause)');
    const offscreen2 = new OffscreenCanvas(7, 6);
    const gl2 = offscreen2.getContext("webgl");
    console.log('created first context (should NOT pause)');
  `);

  testRunner.completeTest();
})
