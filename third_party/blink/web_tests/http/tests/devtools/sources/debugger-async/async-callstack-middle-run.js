// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that capturing asynchronous call stacks in debugger works if started after some time since the page loads.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(timeoutOffCapturing, 0);
      }

      function timeoutOffCapturing()
      {
          setTimeout(timeoutOffCapturing2, 0);
          debugger;
          setTimeout(timeoutOnCapturing, 0);
      }

      function timeoutOffCapturing2()
      {
          debugger;
      }

      function timeoutOnCapturing()
      {
          debugger;
      }
  `);

  var totalDebuggerStatements = 3;
  var maxAsyncCallStackDepth = 8;

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  async function step1() {
    await TestRunner.DebuggerAgent.setAsyncCallStackDepth(0);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  function resumeExecution() {
    SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
  }

  var step = 0;
  var callStacksOutput = [];
  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace) {
    ++step;
    if (step === 1) {
      TestRunner.DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth).then(resumeExecution);
      return;
    }

    callStacksOutput.push(await SourcesTestRunner.captureStackTraceIntoString(callFrames, asyncStackTrace) + '\n');
    if (step < totalDebuggerStatements) {
      resumeExecution();
    } else {
      TestRunner.addResult('Captured call stacks in no particular order:');
      callStacksOutput.sort();
      TestRunner.addResults(callStacksOutput);
      SourcesTestRunner.completeDebuggerTest();
    }
  }
})();
