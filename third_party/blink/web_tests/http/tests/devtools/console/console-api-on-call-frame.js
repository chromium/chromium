// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that command line api does not mask values of scope variables while evaluating on a call frame.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    window.inspect = "inspect";
    var clear = "clear";

    function testFunction()
    {
        var dir = "dir";
        debugger;
    }
  `);
  await TestRunner.showPanel('sources');

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole('dir + clear + inspect', step3);
  }

  function step3(result) {
    TestRunner.addResult('Evaluated in console in the top frame context: dir + clear + inspect = ' + result);
    ConsoleTestRunner.evaluateInConsole('typeof $$', step4);
  }

  function step4(result) {
    TestRunner.addResult('Evaluated in console in the top frame context: typeof $$ = ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
