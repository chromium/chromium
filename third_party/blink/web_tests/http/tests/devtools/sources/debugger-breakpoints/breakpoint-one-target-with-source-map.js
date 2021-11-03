// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that breakpoints work when one target has source map and another does not.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await addWorker(TestRunner.url('resources/worker-with-sourcemap.js'));
  await addWorker(TestRunner.url('resources/worker-with-sourcemap.js'));

  await SourcesTestRunner.startDebuggerTestPromise();

  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('worker-with-sourcemap.ts');
  SourcesTestRunner.createNewBreakpoint(sourceFrame, 2, '', true);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.evaluateInPageAnonymously('window.workers[1].postMessage("")');
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();

  function addWorker(url) {
    return TestRunner.evaluateInPageAsync(`
      (function(){
        window.workers = window.workers || [];
        window.workers.push(new Worker('${url}'));
      })();
    `);
  }
})();
