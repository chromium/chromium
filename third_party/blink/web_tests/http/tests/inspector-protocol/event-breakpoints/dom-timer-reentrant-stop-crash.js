(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that re-entrant clearing of a DOMTimer during DevTools pause does not crash.');

  await dp.Debugger.enable();
  await dp.EventBreakpoints.setInstrumentationBreakpoint({eventName: "clearTimeout"});

  // Set a timer in an iframe, and then immediately clear it.
  // Clearing it will invoke DOMTimer::Stop(), which hits the clearTimeout event breakpoint.
  // During the pause, we remove the iframe. This triggers ContextDestroyed() on the iframe's
  // ExecutionContext, which calls Stop() re-entrantly on all its active observers.
  // Since SetExecutionContext(nullptr) hasn't run yet, the timer receives ContextDestroyed()
  // and calls Stop() again.
  // This causes the nested Stop() to run, setting action_ to null.
  // Then resume, and the outer Stop() proceeds to call action_->Dispose() on the null pointer.

  const evalPromise = session.evaluate(`
    const iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    const win = iframe.contentWindow;
    win.timerId = win.setTimeout(() => {}, 1000);
    win.clearTimeout(win.timerId);
  `);

  const {data, reason} = (await dp.Debugger.oncePaused()).params;
  testRunner.log("Paused on: " + data.eventName);

  // While paused in clearTimeout, remove the iframe.
  await dp.Runtime.evaluate({expression: "document.querySelector('iframe').remove()"});

  await dp.Debugger.resume();
  await evalPromise;

  testRunner.log("Test completed without crashing.");
  testRunner.completeTest();
})
