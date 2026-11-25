// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Runs an async call stack capture test matching legacy
 * SourcesTestRunner.runAsyncCallStacksTest().
 *
 * @param {import('test_runner').TestRunner} testRunner
 * @param {import('test_runner').Session} session
 * @param {import('devtools-protocol/types/protocol-tests-proxy-api.d.ts').ProtocolTestsProxyApi.ProtocolApi}
 *     dp
 * @param {{totalDebuggerStatements: number, maxAsyncCallStackDepth?: number,
 *     setup?: () => Promise<void>}} options
 */
(async function runAsyncCallStacksTest(
    testRunner, session, dp,
    {totalDebuggerStatements, maxAsyncCallStackDepth = 32, setup}) {
  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const args =
        params.args.map(arg => arg.value ?? arg.description ?? '').join(' ');
    testRunner.fail(`Unexpected console.${params.type}: ${args}`);
  });
  dp.Runtime.onExceptionThrown(({params: {exceptionDetails}}) => {
    const details = exceptionDetails.exception?.description || '';
    testRunner.fail(
        `Unexpected exception: ${exceptionDetails.text} ${details}`);
  });

  await dp.Runtime.enable();
  const {result: {debuggerId}} = await dp.Debugger.enable();
  const debuggers = new Map([[debuggerId, dp.Debugger]]);

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: maxAsyncCallStackDepth});

  if (setup) {
    await setup();
  }

  async function nextStackTrace(parent, parentId, state) {
    if (parent) {
      return parent;
    }
    if (!parentId) {
      return null;
    }
    if (parentId.debuggerId) {
      state.debuggerId = parentId.debuggerId;
    }
    const debuggerProtocol = debuggers.get(state.debuggerId);
    if (!debuggerProtocol) {
      testRunner.fail(
          `Unknown debuggerId for async stack trace: ${state.debuggerId}`);
      return null;
    }
    const response =
        await debuggerProtocol.getStackTrace({stackTraceId: parentId});
    if (response.error) {
      testRunner.fail(
          `Debugger.getStackTrace failed: ${JSON.stringify(response.error)}`);
      return null;
    }
    return response.result?.stackTrace ?? null;
  }

  async function formatStackTrace(callFrames, asyncStackTrace,
                                  asyncStackTraceId) {
    const lines = ['Call stack:'];
    function appendFrames(frames) {
      let printed = 0;
      for (const [i, frame] of frames.entries()) {
        const {scriptId, lineNumber} = frame.location || frame;
        const url = (scripts.get(scriptId) || frame.url || '').split('/').pop();
        const name = frame.functionName || '(anonymous)';
        let line = `    ${printed++}) ${name} (${url}:${lineNumber + 1})`;
        line = line.replace(/scheduleTestFunction.+$/,
                            'scheduleTestFunction <omitted>');
        lines.push(line);
        if (frame.functionName === 'scheduleTestFunction') {
          const remainingFrames = frames.length - 1 - i;
          if (remainingFrames) {
            lines.push('    <... skipped remaining frames ...>');
          }
          break;
        }
      }
      return printed;
    }

    appendFrames(callFrames);
    const state = {debuggerId};
    for (let st =
             await nextStackTrace(asyncStackTrace, asyncStackTraceId, state);
         st; st = await nextStackTrace(st.parent, st.parentId, state)) {
      lines.push(`    [${st.description || 'Async Call'}]`);
      const printed = appendFrames(st.callFrames);
      if (!printed) {
        lines.pop();
      }
    }
    return lines.join('\n');
  }

  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate(`
    function scheduleTestFunction() {
      setTimeout(testFunction, 0);
    }
    scheduleTestFunction();
    //# sourceURL=schedule-test-function.js
  `);

  const callStacksOutput = [];
  for (let step = 0; step < totalDebuggerStatements; ++step) {
    const {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
        await pausedPromise;
    callStacksOutput.push(
        await formatStackTrace(callFrames, asyncStackTrace, asyncStackTraceId));
    if (step + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
    } else {
      dp.Debugger.onPaused(() => testRunner.fail('Unexpected extra pause'));
    }
    await dp.Debugger.resume();
  }

  await session.evaluateAsync('new Promise(resolve => setTimeout(resolve, 0))');

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  testRunner.log(callStacksOutput.join('\n\n'));
  testRunner.completeTest();
})
