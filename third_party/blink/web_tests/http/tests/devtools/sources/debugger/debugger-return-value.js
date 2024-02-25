// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests function's return value reported from backend.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function d()
      {
          var i = 10;
          return i;
      }

      function testFunction()
      {
          debugger;
          return d();
      }
  `);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  var stepCount = 0;
  async function step2(callFrames) {
    if (stepCount < 2) {
      for (var i = 0, frame; frame = callFrames[i]; ++i)
        TestRunner.assertTrue(!frame.returnValue(), 'Unexpected returnValue in frame #' + i);
      SourcesTestRunner.stepOver();
      SourcesTestRunner.waitUntilResumed(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step2));
    } else {
      await SourcesTestRunner.captureStackTrace(callFrames, null, {printReturnValue: true});
      SourcesTestRunner.completeDebuggerTest();
    }
    ++stepCount;
  }
})();
