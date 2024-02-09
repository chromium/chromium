// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests stepping into/over/out with framework black-boxing.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          Framework.safeRun(function callback1() {
              Framework.safeRun(Framework.empty, callback2);
          });
      }

      function callback2()
      {
          Framework.safeRun(Framework.empty, Framework.empty); // Should be skipped: all callbacks are inside frameworks.
          Framework.safeRun(Framework.empty, Framework.throwFrameworkException, callback3); // Should be enough to step into callback3
      }

      function callback3()
      {
          var func = Framework.bind(callback4, null, 1);
          func = Framework.bind(func, null, 2);
          func = Framework.bind(func, null, 3);
          Framework.safeRun(func, Framework.empty);
      }

      function callback4()
      {
          Framework.safeRun(Framework.doSomeWork, function() {
              return 0;
          });
          try {
              Framework.throwFrameworkException("message");
          } catch (e) {
              window.ex = e;
          }
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
      'Print',                                                  // debugger;
      'StepInto', 'StepInto', 'Print',                          // callback1
      'StepInto', 'Print',                                      // callback2
      'StepInto', 'Print',                                      // callback2, skipped
      'StepInto', 'Print',                                      // callback3
      'StepInto', 'StepInto', 'StepInto', 'StepInto', 'Print',  // callback4
      'StepInto', 'Print',                                      // anonymous calback
      'StepInto', 'StepInto', 'Print',                          // callback4, skipped
      'StepInto', 'Print',                                      // callback4, inside catch
      'StepOut',  'Print',                                      // return to callback3
      'StepOver', 'Print',                                      // return to callback2
      'StepInto', 'Print',                                      // return to callback1
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
