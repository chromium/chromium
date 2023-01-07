// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger won't stop on syntax errors even if "pause on uncaught exceptions" is on.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');

  SourcesTestRunner.startDebuggerTest(step1);

  async function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    await TestRunner.addIframe('resources/syntax-error.html');
    await ConsoleTestRunner.dumpConsoleMessages();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
