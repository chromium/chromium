// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
    `Tests that when stack overflow exception happens when inspector is open the stack trace is correctly shown in console.\n`
  );

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    // Both the call and the function entry may trigger stack overflow.
    // Intentionally keep both on the same line to avoid flaky test failures.
    function overflow() { overflow(); }

    function doOverflow()
    {
      setTimeout(overflow, 0);
    }
  `);

  TestRunner.evaluateInPage('doOverflow()', step2.bind(this));

  function step2() {
    if (Console.ConsoleView.instance().visibleViewMessages.length < 1) ConsoleTestRunner.addConsoleSniffer(step2);
    else step3();
  }

  function step3() {
    ConsoleTestRunner.expandConsoleMessages(onExpanded);
  }

  async function onExpanded() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
