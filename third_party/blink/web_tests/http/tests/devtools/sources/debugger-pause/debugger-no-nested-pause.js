// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that debugger will skip breakpoint hit when script execution is already paused. See bug\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          testFunction.invocationCount++;
          debugger;
      }

      testFunction.invocationCount = 0;
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole(
        'testFunction(); testFunction.invocationCount', step3);
    TestRunner.addResult('Set timer for test function.');
  }

  function step3(result) {
    TestRunner.addResult('testFunction.invocationCount = ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
