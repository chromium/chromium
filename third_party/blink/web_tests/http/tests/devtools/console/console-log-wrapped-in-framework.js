// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests console.log() anchor location when the skip-stack-frames feature is enabled.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function runLogs()
      {
          console.log("direct console.log()");
          Framework.log("framework log");
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);

  TestRunner.evaluateInPage('runLogs()');
  TestRunner.deprecatedRunAfterPendingDispatches(callback);
  async function callback() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
