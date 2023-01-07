// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that evaluation in the context of top frame will see values of its local variables, even if there are global variables with same names. On success the test will print a = 2(value of the local variable a). Bug 47358.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var a = 1;
      function testFunction()
      {
          var a = 2;
          debugger;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole('a', step3);
  }

  function step3(result) {
    TestRunner.addResult('Evaluated in console in the top frame context: a = ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
