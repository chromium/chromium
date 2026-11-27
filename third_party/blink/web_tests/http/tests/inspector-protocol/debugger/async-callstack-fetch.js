// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for fetch.\n');

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  const scriptUrls = new Map();
  dp.Debugger.onScriptParsed(event => {
    scriptUrls.set(event.params.scriptId, event.params.url);
  });

  const failures = [];
  dp.Runtime.onExceptionThrown(event => {
    failures.push(`Unexpected exception: ${JSON.stringify(event.params)}`);
  });
  dp.Runtime.onConsoleAPICalled(event => {
    failures.push(`Unexpected console.${event.params.type}: ${
        JSON.stringify(event.params.args)}`);
  });
  let fetchStatus = 0;
  dp.Network.onResponseReceived(event => {
    if (event.params.response.url.includes('script-url.js')) {
      fetchStatus = event.params.response.status;
    }
  });
  dp.Network.onLoadingFailed(event => {
    failures.push(
        `Unexpected loading failure: ${JSON.stringify(event.params)}`);
  });

  function formatCallFrames(callFrames, asyncStackTrace) {
    const lines = ['Call stack:'];

    function appendFrames(frames, isRuntimeFrames) {
      for (let i = 0; i < frames.length; ++i) {
        const frame = frames[i];
        const scriptId =
            isRuntimeFrames ? frame.scriptId : frame.location.scriptId;
        const rawUrl = scriptUrls.get(scriptId) || frame.url || '';
        const url = rawUrl ? rawUrl.split('/').pop() : '';
        const line =
            (isRuntimeFrames ? frame.lineNumber : frame.location.lineNumber) +
            1;
        if (frame.functionName === 'scheduleTestFunction') {
          lines.push(`    ${i}) scheduleTestFunction <omitted>`);
          if (frames.length - 1 - i > 0) {
            lines.push('    <... skipped remaining frames ...>');
          }
          return true;
        }
        lines.push(`    ${i}) ${frame.functionName} (${url}:${line})`);
      }
      return frames.length > 0;
    }

    appendFrames(callFrames, false);
    for (let trace = asyncStackTrace; trace; trace = trace.parent) {
      lines.push(`    [${trace.description || 'Async Call'}]`);
      if (!appendFrames(trace.callFrames, true)) {
        lines.pop();
      }
    }
    return lines.join('\n') + '\n';
  }

  await session.evaluate(`${'\n'.repeat(10)}
      function testFunction()
      {
          setTimeout(doFetch, 0);
      }

      function doFetch()
      {
          fetch('${
      testRunner.url('./resources/script-url.js')}').then(function chained1() {
              debugger;
          }).then(function chained2() {
          }).then(function chained3() {
          }).then(function chained4() {
              debugger;
          });
      }
      //# sourceURL=async-callstack-fetch.js
  `);

  await session.evaluate(`
      function scheduleTestFunction() {
        setTimeout(testFunction, 0);
      }
  `);

  const totalDebuggerStatements = 2;
  const callStacksOutput = [];
  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate('scheduleTestFunction()');

  for (let step = 0; step < totalDebuggerStatements; ++step) {
    const {params} = await pausedPromise;
    if (params.asyncStackTraceId) {
      failures.push(`Unexpected asyncStackTraceId: ${
          JSON.stringify(params.asyncStackTraceId)}`);
    }
    callStacksOutput.push(
        formatCallFrames(params.callFrames, params.asyncStackTrace));
    pausedPromise =
        step + 1 < totalDebuggerStatements ? dp.Debugger.oncePaused() : null;
    await dp.Debugger.resume();
  }

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }

  let extraPause = false;
  dp.Debugger.onPaused(() => {
    extraPause = true;
    dp.Debugger.resume();
  });
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 0});
  await dp.Debugger.setBreakpointsActive({active: true});
  await session.evaluate('new Promise(resolve => setTimeout(resolve, 0))');
  if (extraPause) {
    failures.push('Unexpected extra pause after completion');
  }
  if (fetchStatus !== 200) {
    failures.push(
        `Expected fetch HTTP 200 for script-url.js, got ${fetchStatus}`);
  }
  if (failures.length) {
    testRunner.log(`FAIL: ${failures.join('; ')}`);
  }

  testRunner.completeTest();
});
