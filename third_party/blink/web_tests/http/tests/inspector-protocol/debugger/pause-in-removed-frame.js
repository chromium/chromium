// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests pause functionality in detached frame.');

  const scriptUrls = new Map();
  dp.Debugger.onScriptParsed(event => {
    scriptUrls.set(event.params.scriptId, event.params.url);
  });

  function formatLocation(callFrame) {
    const rawUrl =
        callFrame.url || scriptUrls.get(callFrame.location.scriptId) || '';
    const displayUrl = rawUrl.replace(/^http:\/\/127\.0\.0\.1(?=:8000\/)/, '');
    const lineNumber = callFrame.location.lineNumber + 1;
    return `${displayUrl}:${lineNumber}`;
  }

  function logStackTrace(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; i++) {
      const frame = callFrames[i];
      const functionName = frame.functionName || '(anonymous)';
      testRunner.log(`    ${i}) ${functionName} (${formatLocation(frame)})`);
    }
  }

  dp.Runtime.onConsoleAPICalled(event => {
    const args = event.params.args.map(a => a.value ?? a.description).join(' ');
    testRunner.log(`FAIL: unexpected console.${event.params.type}: ${args}`);
  });
  dp.Runtime.onExceptionThrown(event => {
    testRunner.log(`FAIL: uncaught exception: ${
        JSON.stringify(event.params.exceptionDetails)}`);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const childUrl = testRunner.url('resources/removed-frame-child.html');
  await session.evaluateAsync(`
    new Promise(resolve => {
      const iframe = document.createElement('iframe');
      iframe.id = 'child';
      iframe.onload = resolve;
      iframe.src = ${JSON.stringify(childUrl)};
      document.body.appendChild(iframe);
    })
  `);

  await session.evaluate(`
      window.removeIframe = function()
      {
        var child = document.getElementById('child');
        child.parentNode.removeChild(child);
        debugger;
      };

      function testFunction()
      {
          var childWindow = document.getElementById("child").contentWindow;
          try {
            childWindow.sendRequest();
          } catch (e) {
            if (e.name !== 'NetworkError') {
              throw e;
            }
          } finally {
            if (window.__doneResolve) {
              window.__doneResolve();
            }
          }
      }

      function scheduleTestFunction()
      {
        window.__donePromise = new Promise(resolve => {
          window.__doneResolve = resolve;
        });
        setTimeout(testFunction, 0);
        return window.__donePromise;
      }
      //# sourceURL=pause-in-removed-frame.js
  `);

  let pauseCount = 0;
  let pausedResolve;
  const pausedPromise = new Promise(resolve => {
    pausedResolve = resolve;
  });

  dp.Debugger.onPaused(async event => {
    pauseCount++;
    if (pauseCount > 1) {
      testRunner.log(`FAIL: Unexpected pause #${pauseCount} at ${
          formatLocation(event.params.callFrames[0])}`);
      await dp.Debugger.resume();
      return;
    }
    testRunner.log('Script execution paused.');
    logStackTrace(event.params.callFrames);
    await dp.Debugger.resume();
    testRunner.log('Script execution resumed.');
    pausedResolve();
  });

  testRunner.log('Set timer for test function.');
  const evalDone = dp.Runtime.evaluate({
    expression: 'scheduleTestFunction()',
    awaitPromise: true,
    returnByValue: true,
  });

  await pausedPromise;
  const evalResult = await evalDone;
  if (evalResult.error || evalResult.result?.exceptionDetails) {
    testRunner.log(`FAIL: evaluate error: ${JSON.stringify(evalResult)}`);
  }
  if (pauseCount !== 1) {
    testRunner.log(`FAIL: expected exactly 1 pause, got ${pauseCount}`);
  }
  testRunner.completeTest();
})
