// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      'Tests event listener breakpoint to break on the first statement of new scripts.');

  await dp.Debugger.enable();
  await dp.Runtime.enable();

  const scriptUrls = new Map();
  dp.Debugger.onScriptParsed(event => {
    scriptUrls.set(event.params.scriptId, event.params.url);
  });

  await session.evaluate(`
      function testFunction() {
        setTimeout(callback1, 0);
      }

      function callback1() {
        var code =
            '(function() {\\n' +
                'setTimeout(callback2, 0);\\n' +
            '})();\\n' +
            '//# sourceURL=inline_callback1.js';
        setTimeout(code, 0);
      }

      function callback2() {
        var script = document.createElement('script');
        script.src = '${testRunner.url('resources/script-first-stmt.js')}';
        document.body.appendChild(script);
        script.remove();
      }
  `);

  function logPause(params) {
    const eventName = params.data?.eventName ?? '';
    testRunner.log(`\nPaused on ${eventName}`);
    testRunner.log('Call stack:');
    params.callFrames.forEach((frame, index) => {
      const rawUrl = scriptUrls.get(frame.location.scriptId) || frame.url || '';
      const url = rawUrl.split('/').pop();
      testRunner.log(`    ${index}) ${frame.functionName} (${url}:${
          frame.location.lineNumber + 1})`);
    });
  }

  await dp.DOMDebugger.setInstrumentationBreakpoint(
      {eventName: 'scriptFirstStatement'});

  const firstPausePromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0);');
  testRunner.log('\nSet timer for test function.');

  const firstPause = await firstPausePromise;
  logPause(firstPause.params);

  const secondPausePromise = dp.Debugger.oncePaused();
  await dp.Debugger.resume();

  const secondPause = await secondPausePromise;
  logPause(secondPause.params);

  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;

  await dp.DOMDebugger.removeInstrumentationBreakpoint(
      {eventName: 'scriptFirstStatement'});

  let pausedAfterRemove = false;
  const onUnexpectedPause = () => {
    pausedAfterRemove = true;
    testRunner.log(
        'FAIL: paused unexpectedly after removeInstrumentationBreakpoint');
    dp.Debugger.resume();
  };
  dp.Debugger.onPaused(onUnexpectedPause);

  const ranAfterRemove = await session.evaluateAsync(`
      new Promise(resolve => {
        window.__afterRemoveResolve = resolve;
        setTimeout(
            'window.__afterRemoveResolve(true); //# sourceURL=after_remove.js',
            0);
      });
  `);
  testRunner.log(`\nSubsequent script completed without pausing: ${
      ranAfterRemove && !pausedAfterRemove}`);

  dp.Debugger.offPaused(onUnexpectedPause);
  testRunner.completeTest();
});
