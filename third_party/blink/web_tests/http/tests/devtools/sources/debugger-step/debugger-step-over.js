// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests "step over" functionality in debugger.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function f()
      {
          var i = 10;
      }

      function d()
      {
          debugger;
          f();
      }

      function testFunction()
      {
          d();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  var stepCount = 0;
  async function step2(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    if (stepCount < 2) {
      TestRunner.addResult('Stepping over...');
      SourcesTestRunner.stepOver();
      SourcesTestRunner.waitUntilResumed(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step2));
    } else
      SourcesTestRunner.completeDebuggerTest();
    stepCount++;
  }
})();
