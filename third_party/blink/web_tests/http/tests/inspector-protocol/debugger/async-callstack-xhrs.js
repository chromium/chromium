// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests asynchronous call stacks for XHRs.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.fail(`Unexpected runtime exception: ${JSON.stringify(params)}`);
  });

  const {result: {debuggerId}} = await dp.Debugger.enable();
  const debuggers = new Map([[debuggerId, dp.Debugger]]);

  const setupResult = await dp.Runtime.evaluate({
    expression: `${'\n'.repeat(10)}
      var xhrCount = 0;

      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          sendSyncXHR();
          sendAsyncXHR();
      }

      function sendAsyncXHR() { sendXHR(true); }
      function sendSyncXHR() { sendXHR(false); }

      function sendXHR(async)
      {
          var xhr = new XMLHttpRequest();
          xhr.onreadystatechange = function()
          {
              if (xhr.readyState == 4) {
                  xhr.onreadystatechange = null;
                  debugger;
              }
          };
          function downloadEnd1()
          {
              xhr.removeEventListener("loadend", downloadEnd1, false);
              debugger;
          }
          function downloadEnd2()
          {
              xhr.removeEventListener("loadend", downloadEnd2, true);
              debugger;
          }
          function uploadEnd()
          {
              xhr.upload.removeEventListener("loadend", uploadEnd, false);
              debugger;
          }
          function downloadProgress()
          {
              debugger;
              xhr.removeEventListener("progress", downloadProgress, false);
          }
          function uploadProgress()
          {
              debugger;
              xhr.upload.removeEventListener("progress", uploadProgress, false);
          }
          xhr.addEventListener("loadend", downloadEnd1, false);
          xhr.addEventListener("loadend", downloadEnd2, true);
          if (async) {
              xhr.upload.addEventListener("loadend", uploadEnd, false);
              xhr.addEventListener("progress", downloadProgress, false);
              xhr.upload.addEventListener("progress", uploadProgress, false);
          }
          xhr.open("POST", "/foo?count=" + (++xhrCount) + "&now=" + Date.now(), async);
          xhr.send(String(sendXHR));
      }
      //# sourceURL=async-callstack-xhrs.js
  `,
  });
  if (setupResult.error || setupResult.result.exceptionDetails) {
    testRunner.fail(
        `Unexpected setup evaluation failure: ${JSON.stringify(setupResult)}`);
    return;
  }

  // Keep scheduleTestFunction() in an anonymous helper script so the outer
  // [setTimeout] frame matches the legacy DebuggerTestRunner wrapper behavior.
  const scheduleSetupResult = await dp.Runtime.evaluate({
    expression: `
      function scheduleTestFunction() {
        setTimeout(testFunction, 0);
      }
    `,
  });
  if (scheduleSetupResult.error ||
      scheduleSetupResult.result.exceptionDetails) {
    testRunner.fail(`Unexpected schedule setup failure: ${
        JSON.stringify(scheduleSetupResult)}`);
    return;
  }

  async function formatStackTrace(callFrames, asyncStackTrace,
                                  asyncStackTraceId) {
    const lines = ['Call stack:'];

    function appendFrames(frames) {
      let printed = 0;
      for (let i = 0; i < frames.length; i++) {
        const frame = frames[i];
        const location = frame.location || frame;
        const url = (scripts.get(location.scriptId) || frame.url || '')
                        .split('/')
                        .pop();
        const lineNumber = location.lineNumber + 1;
        let line =
            `    ${printed++}) ${frame.functionName} (${url}:${lineNumber})`;
        line = line.replace(/scheduleTestFunction.+$/,
                            'scheduleTestFunction <omitted>');
        lines.push(line);

        if (frame.functionName === 'scheduleTestFunction') {
          if (i < frames.length - 1) {
            lines.push('    <... skipped remaining frames ...>');
          }
          break;
        }
      }
      return printed;
    }

    appendFrames(callFrames);

    let stackTrace = asyncStackTrace;
    let stackTraceId = asyncStackTraceId;
    while (stackTrace || stackTraceId) {
      if (!stackTrace && stackTraceId) {
        const debuggerProtocol = (stackTraceId.debuggerId &&
                                  debuggers.get(stackTraceId.debuggerId)) ||
            dp.Debugger;
        const {result} = await debuggerProtocol.getStackTrace({stackTraceId});
        stackTrace = result.stackTrace;
      }
      lines.push(`    [${stackTrace.description || 'Async Call'}]`);
      if (!appendFrames(stackTrace.callFrames)) {
        lines.pop();
      }
      stackTraceId = stackTrace.parentId;
      stackTrace = stackTrace.parent;
    }

    return lines.join('\n') + '\n';
  }

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  const totalDebuggerStatements = 9;
  const callStacksOutput = [];

  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  const triggerPromise = session.evaluate('scheduleTestFunction()');

  for (let step = 0; step < totalDebuggerStatements; step++) {
    const {
      params: {callFrames, asyncStackTrace, asyncStackTraceId},
    } = await pausedPromise;
    callStacksOutput.push(
        await formatStackTrace(callFrames, asyncStackTrace, asyncStackTraceId));

    if (step + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
      await dp.Debugger.resume();
    }
  }
  await triggerPromise;

  let pausedAfterFinish = false;
  dp.Debugger.onPaused(() => {
    pausedAfterFinish = true;
  });
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;

  if (pausedAfterFinish) {
    testRunner.fail('Unexpected extra pause after expected 9 pauses.');
    return;
  }

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }
  testRunner.completeTest();
});
