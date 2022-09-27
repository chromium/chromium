// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that blackboxed script will be skipped while stepping on worker.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function installWorker()
      {
          new Worker("../resources/worker-source.js");
      }
  `);

  var frameworkRegexString = 'foo\\.js$';
  Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);

  SourcesTestRunner.startDebuggerTest(step1, true);
  function step1() {
    var actions = ['StepOver', 'StepInto', 'Print'];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, () => SourcesTestRunner.completeDebuggerTest());
    TestRunner.evaluateInPage('installWorker()');
  }
})();
