// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that stepping into blackboxed framework will not pause on setTimeout() inside the framework.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      var counter = 0;

      function testFunction()
      {
          Framework.scheduleUntilDone(callback, 0);
      }

      function callback()
      {
          ++counter;
          if (counter === 1)
              stop();
          return counter === 2;
      }

      function stop()
      {
          debugger;
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    var actions = [
      'Print',                                                       // debugger;
      'StepOut', 'Print', 'StepInto', 'Print', 'StepInto', 'Print',  // Should NOT stop on setTimeout() inside framework
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
