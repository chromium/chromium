// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests event listener breakpoint to break on the first statement of new modules.');
  testRunner.log('');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(event => {
    scripts.set(event.params.scriptId, event.params);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const moduleUrl = testRunner.url('resources/empty-module.js');
  const setupRes = await dp.Runtime.evaluate({
    expression: `
      function testFunction(url = ${JSON.stringify(moduleUrl)}) {
        const module = document.createElement('script');
        module.type = 'module';
        module.src = url;
        document.body.appendChild(module);
        module.remove();
      }
      function scheduleTestFunction() {
        setTimeout(testFunction, 0);
      }
    `,
    objectGroup: 'console',
  });
  if (setupRes.error || setupRes.result.exceptionDetails) {
    testRunner.fail(`Unexpected setup failure: ${JSON.stringify(setupRes)}`);
    return;
  }

  await dp.EventBreakpoints.setInstrumentationBreakpoint({
    eventName: 'scriptFirstStatement',
  });

  const pausedPromise = dp.Debugger.oncePaused();
  const scheduleRes = await dp.Runtime.evaluate({
    expression: 'scheduleTestFunction()',
    objectGroup: 'console',
  });
  if (scheduleRes.error || scheduleRes.result.exceptionDetails) {
    testRunner.fail(
        `Unexpected schedule failure: ${JSON.stringify(scheduleRes)}`);
    return;
  }
  testRunner.log('Set timer for test function.');

  const {params: {callFrames, reason, data}} = await pausedPromise;
  if (reason !== 'EventListener') {
    testRunner.fail(`Expected pause reason EventListener, got ${reason}`);
    return;
  }
  const eventName =
      ((data && data.eventName) || '').replace(/^instrumentation:/, '');
  if (!callFrames.length) {
    testRunner.fail(
        'Expected non-empty callFrames on scriptFirstStatement pause.');
    return;
  }
  testRunner.log('');
  testRunner.log(`Paused on ${eventName}`);
  testRunner.log('Call stack:');
  for (let i = 0; i < callFrames.length; i++) {
    const frame = callFrames[i];
    const script = scripts.get(frame.location.scriptId);
    if (!script || !script.isModule) {
      testRunner.fail(
          `Expected module script frame, got ${JSON.stringify(script)}`);
      return;
    }
    const url = (script.url || frame.url || '').split('/').pop();
    testRunner.log(`    ${i}) ${frame.functionName} (${url}:${
        frame.location.lineNumber + 1})`);
  }

  // Remove the instrumentation breakpoint before resuming because both
  // ModuleRecord::Instantiate() and V8ScriptRunner::EvaluateModule() enter
  // probe::ExecuteScript, so EvaluateModule() would pause again on the same
  // module before removeInstrumentationBreakpoint arrives after resumed.
  let pausedAfterRemove = false;
  dp.Debugger.onPaused(() => {
    pausedAfterRemove = true;
  });

  await dp.EventBreakpoints.removeInstrumentationBreakpoint({
    eventName: 'scriptFirstStatement',
  });

  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;

  // Verify subsequent module execution does not pause after breakpoint removal.
  const secondModuleUrl =
      testRunner.url('resources/empty-module.js?after-remove');
  const secondParsedPromise = dp.Debugger.onceScriptParsed(
      event => event.params.url === secondModuleUrl && event.params.isModule);
  const afterRemoveRes = await dp.Runtime.evaluate({
    expression: `new Promise((resolve, reject) => {
      const module = document.createElement('script');
      module.type = 'module';
      module.src = ${JSON.stringify(secondModuleUrl)};
      module.onload = () => resolve();
      module.onerror = () => reject(new Error('Failed to load module'));
      document.body.appendChild(module);
    })`,
    awaitPromise: true,
    objectGroup: 'console',
  });
  if (afterRemoveRes.error || afterRemoveRes.result.exceptionDetails) {
    testRunner.fail(`Unexpected post-removal module failure: ${
        JSON.stringify(afterRemoveRes)}`);
    return;
  }
  await secondParsedPromise;
  if (pausedAfterRemove) {
    testRunner.fail(
        'Unexpected pause after removing scriptFirstStatement breakpoint.');
    return;
  }

  testRunner.completeTest();
});
