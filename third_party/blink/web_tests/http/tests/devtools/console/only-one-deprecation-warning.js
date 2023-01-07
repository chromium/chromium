// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test passes if only one deprecation warning is presented in the console.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.evaluateInPagePromise(`
    var x = window.webkitStorageInfo;
    var y = window.webkitStorageInfo;
  `);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
