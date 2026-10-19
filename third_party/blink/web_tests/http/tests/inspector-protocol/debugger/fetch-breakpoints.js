// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests DOMDebugger.setXHRBreakpoint and DOMDebugger.removeXHRBreakpoint with fetch().`);

  await dp.Debugger.enable();
  await session.evaluate(`
      function sendRequest(url)
      {
          fetch(url);
      }
//# sourceURL=fetch-breakpoints.js`);

  let pausedUnexpectedly = false;
  dp.Debugger.onPaused(() => {
    if (pausedUnexpectedly) {
      testRunner.log('Fail, paused again after breakpoint was removed.');
    }
  });

  async function triggerAndWaitForPause(expression, dumpStack = false) {
    pausedUnexpectedly = false;
    const pausedPromise = dp.Debugger.oncePaused();
    const evalPromise = session.evaluate(expression);
    const {params} = await pausedPromise;
    testRunner.log('Script execution paused.');
    if (dumpStack) {
      testRunner.log('Call stack:');
      params.callFrames.forEach((frame, i) => {
        const url = frame.url ? frame.url.split('/').pop() : '';
        testRunner.log(`    ${i}) ${frame.functionName} (${url}:${
            frame.location.lineNumber + 1})`);
      });
    }
    await dp.Debugger.resume();
    await evalPromise;
    testRunner.log('Script execution resumed.');
    pausedUnexpectedly = true;
  }

  await testRunner.runTestSuite([
    async function testFetchBreakpoint() {
      await dp.DOMDebugger.setXHRBreakpoint({url: 'foo'});
      await triggerAndWaitForPause(`sendRequest('/foo?a=b')`, true);
      await session.evaluate(`sendRequest('/bar?a=b')`);
      await dp.DOMDebugger.removeXHRBreakpoint({url: 'foo'});
      await session.evaluate(`sendRequest('/foo?a=b')`);
    },

    async function testPauseOnAnyFetch() {
      await dp.DOMDebugger.setXHRBreakpoint({url: ''});
      await triggerAndWaitForPause(`sendRequest('/foo?a=b')`);
      await triggerAndWaitForPause(`sendRequest('/bar?a=b')`);
      await dp.DOMDebugger.removeXHRBreakpoint({url: ''});
      await session.evaluate(`sendRequest('/baz?a=b')`);
    },

    async function testDisableBreakpoint() {
      await dp.DOMDebugger.setXHRBreakpoint({url: ''});
      await triggerAndWaitForPause(`sendRequest('/foo')`);
      await dp.DOMDebugger.removeXHRBreakpoint({url: ''});
      await session.evaluate(`sendRequest('/foo')`);
    },
  ]);
});
