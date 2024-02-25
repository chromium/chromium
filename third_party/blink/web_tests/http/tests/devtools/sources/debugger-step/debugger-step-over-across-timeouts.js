// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that debugger StepOver will stop inside next timeout handler.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(callback1, 0);
      }

      function callback1()
      {
          setTimeout(callback2, 0);
          debugger;
      }

      function callback2()
      {
          var dummy = 42; // Should pause here.
          (function FAIL_Should_Not_Pause_Here() { debugger; })();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    var actions = [
      'Print',  // "debugger" in callback1
      'StepOver',
      'StepOver',
      'Print',
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
        actions, () => SourcesTestRunner.completeDebuggerTest());
  }
})();
