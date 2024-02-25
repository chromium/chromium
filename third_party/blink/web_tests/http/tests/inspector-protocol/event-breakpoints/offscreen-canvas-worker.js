(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that CanvasCreated breakpoing is hit when offscreen context is created in a worker');

  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  session.evaluate(`
    const script = \`
      const offscreen = new OffscreenCanvas(42, 42);
      const gl = offscreen.getContext("webgl");
    \`;
    const blob = new Blob([script], {type: 'application/javascript'});
    const worker = new Worker(URL.createObjectURL(blob));
  `);

  const workerTarget = (await dp.Target.onceAttachedToTarget()).params;
  const workerSession = session.createChild(workerTarget.sessionId);
  const wp = workerSession.protocol;

  wp.Debugger.enable();
  wp.EventBreakpoints.setInstrumentationBreakpoint({eventName: "canvasContextCreated"});
  wp.Runtime.runIfWaitingForDebugger();
  const {data, reason} = (await wp.Debugger.oncePaused()).params;
  testRunner.log({data, reason});
  testRunner.completeTest();
})
