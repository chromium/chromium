// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests to ensure datsaver logs warning in console if enabled and only shown once on reloads.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  TestRunner.addResult('Console messages:');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.addResult('');

  TestRunner.addResult('Enabling data saver');
  TestRunner.evaluateInPagePromise('internals.setSaveDataEnabled(true)');
  TestRunner.addResult('Reloading Page');
  await TestRunner.reloadPagePromise();

  TestRunner.addResult('Console messages:');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.addResult('');

  TestRunner.addResult('Reloading Page');
  await TestRunner.reloadPagePromise();
  TestRunner.addResult('Console messages:');
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
