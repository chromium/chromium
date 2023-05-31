// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests provisional ignore-listing.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          eval("239;//# sourceURL=framework.js");
      }
      //# sourceURL=frameworks-ignore-list-by-source-code.js
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    TestRunner.addSniffer(Bindings.IgnoreListManager.prototype, 'patternChangeFinishedForTests', step2);
    var frameworkRegexString = '^framework\\.js$';
    Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);
  }

  function step2() {
    var actions = [
      'Print',  // "debugger" in testFunction()
      'StepInto', 'StepInto', 'Print'
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step4);
    SourcesTestRunner.runTestFunction();
  }

  function step4() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
