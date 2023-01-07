// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the console can preserve log messages across cross-process navigations.`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('http://devtools.oopif.test:8000/devtools/console/resources/log-message.html')
  Common.settingForTest('preserveConsoleLog').set(true);
  await TestRunner.evaluateInPage(`logMessage('before navigation')`);
  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/console/resources/log-message.html')
  await TestRunner.evaluateInPage(`logMessage('after navigation')`);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
