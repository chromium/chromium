// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests framework ignore listing feature with sourcemaps.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../debugger/resources/framework-with-sourcemap.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          return foo(callback);
      }

      function callback(i)
      {
          return i;
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.Settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    var actions = [
      'Print',                          // "debugger" in testFunction()
      'StepInto', 'StepInto', 'Print',  // entered callback(i)
      'StepOut', 'Print'
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
