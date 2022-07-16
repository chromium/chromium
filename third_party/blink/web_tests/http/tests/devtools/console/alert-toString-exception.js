// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that browser won't crash if inspector is opened for a page that fails to convert alert() argument to string. The test passes if it doesn't crash. Bug 60541\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('resources/alert-toString-exception.html');
  await TestRunner.reloadPagePromise();

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
