(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that instrumentation breakpoints are preserved after cross-origin navigation');

  dp.Debugger.enable();
  await dp.EventBreakpoints.setInstrumentationBreakpoint({eventName: "canvasContextCreated"});
  await session.navigate('http://devtools.test:8000/inspector-protocol/resources/inspector-protocol-page.html');
  session.evaluate(`
    const offscreen = new OffscreenCanvas(42, 42);
    const gl = offscreen.getContext("webgl");
  `);
  const {data, reason} = (await dp.Debugger.oncePaused()).params;
  testRunner.log({data, reason});
  testRunner.completeTest();
})
