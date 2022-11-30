// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that expression which is evaluated as Object Literal, is correctly stored in console history. (crbug.com/584881)\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('{a:1, b:2}', step2);

  function step2() {
    var consoleView = Console.ConsoleView.instance();
    TestRunner.addResult(consoleView.prompt.history().previous());
    TestRunner.completeTest();
  }
})();
