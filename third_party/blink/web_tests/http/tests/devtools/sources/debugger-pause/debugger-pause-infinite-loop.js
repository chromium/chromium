// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that we can break infinite loop started from console.`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise('a = true');

  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.addResult('Run infinite loop');
  ConsoleTestRunner.evaluateInConsole(`while(a) {}; 42`);
  TestRunner.addResult('Request pause');
  SourcesTestRunner.togglePause();
  await SourcesTestRunner.waitUntilPausedPromise();
  TestRunner.addResult('Change condition on pause to finish infinite loop');
  ConsoleTestRunner.evaluateInConsole('a = false');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(3);
  await new Promise(resolve => SourcesTestRunner.resumeExecution(resolve));
  await ConsoleTestRunner.waitForConsoleMessagesPromise(4);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.addResult('Infinite loop finished');
  SourcesTestRunner.completeDebuggerTest();
})();
