// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console.memory returns fresh instance/samples.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('console.memory === console.memory', step1);

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
