// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests stopping in debugger in the worker with source mapping.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function installWorker()
      {
          new Worker("resources/worker-compiled.js");
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.evaluateInPage('installWorker()');
    SourcesTestRunner.waitUntilPaused(paused);
    TestRunner.addSniffer(Bindings.CompilerScriptMapping.prototype, 'sourceMapAttachedForTest', sourceMapLoaded);
  }

  var callFrames;
  var callbacksLeft = 2;

  function paused(callFramesParam) {
    callFrames = callFramesParam;
    maybeFinishTest();
  }

  function sourceMapLoaded() {
    maybeFinishTest();
  }

  async function maybeFinishTest() {
    if (--callbacksLeft)
      return;
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
