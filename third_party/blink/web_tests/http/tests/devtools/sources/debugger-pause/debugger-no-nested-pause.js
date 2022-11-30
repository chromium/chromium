// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger will skip breakpoint hit when script execution is already paused. See bug\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
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
