// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank('Checks stepInto fetch');

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const scriptUrls = new Map();
  dp.Debugger.onScriptParsed(event => {
    scriptUrls.set(event.params.scriptId, event.params.url);
  });

  const pausedPromise = dp.Debugger.oncePaused();
  const evalPromise = dp.Runtime.evaluate({
    expression: `
    debug(fetch);
    fetch("${testRunner.url('./resources/script-url.js')}");
    //# sourceURL=test.js`,
    includeCommandLineAPI: true,
    objectGroup: 'console',
    replMode: true,
  });

  const {params} = await pausedPromise;
  testRunner.log('Script execution paused.');

  testRunner.log('Call stack:');
  params.callFrames.forEach((frame, index) => {
    const url = scriptUrls.get(frame.location.scriptId) || frame.url;
    testRunner.log(`    ${index}) ${frame.functionName} (${url}:${
        frame.location.lineNumber + 1})`);
  });

  const resumedPromise = dp.Debugger.onceResumed();
  const stepPausedPromise = dp.Debugger.oncePaused();
  await dp.Debugger.stepInto({breakOnAsyncCall: true});
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  // The step ends at the end of test.js because fetch() schedules no
  // async task to break on. Resume so that the evaluation can finish.
  await stepPausedPromise;
  await dp.Debugger.resume();
  await evalPromise;

  testRunner.completeTest();
});
