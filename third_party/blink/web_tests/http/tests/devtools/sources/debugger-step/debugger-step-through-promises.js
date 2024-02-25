// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that debugger will step through Promise handlers while not stepping into V8 internal scripts.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          Promise.resolve(42).then(
              function p1()
              {
                  debugger;
              }
          ).then(
              function p2()
              {
                  return window.foo || 1;
              }
          ).then(
              function p3()
              {
                  return window.foo || 2;
              }
          ).catch(function(e) {
              console.error("FAIL: Unexpected exception: " + e);
          });
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    var actions = [
      'Print',  // debugger; at p1
      'StepInto',
      'Print',
      'StepInto',
      'Print',  // entered p2
      'StepOver',
      'Print',
      'StepOver',
      'Print',  // entered p3
      'StepOver',
      'Print',
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
