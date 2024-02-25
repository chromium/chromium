(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that CanvasCreated breakpoing is hit when offscreen context is created in the page');

  dp.Debugger.enable();
  dp.EventBreakpoints.setInstrumentationBreakpoint({eventName: "canvasContextCreated"});
  session.evaluate(`
    const offscreen = new OffscreenCanvas(42, 42);
    const gl = offscreen.getContext("webgl");
  `);
  const {data, reason} = (await dp.Debugger.oncePaused()).params;
  testRunner.log({data, reason});
  testRunner.completeTest();
})
