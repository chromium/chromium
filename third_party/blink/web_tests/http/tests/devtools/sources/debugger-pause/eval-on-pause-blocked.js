// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that evaluation in the context of top frame will not be blocked by Content-Security-Policy. Bug 77203.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var foo = 2012;
          debugger;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole('foo', step3);
  }

  function step3(result) {
    TestRunner.addResult(
        'Evaluated in console in the top frame context: foo = ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
