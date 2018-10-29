// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.setupStartupTest('resources/console-promise-reject-and-handle.html');
  TestRunner.addResult(`Tests that evt.preventDefault() in window.onunhandledrejection suppresses console output.\n`);
  await TestRunner.loadModule('console_test_runner');
  Console.ConsoleView.instance()._setImmediatelyFilterMessagesForTest();

  ConsoleTestRunner.expandConsoleMessages();
  TestRunner.addResult('----console messages start----');
  ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.addResult('----console messages end----');

  // Turn on verbose filter.
  TestRunner.addResult(`\nEnable verbose filter`);
  Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.allLevelsFilterValue());
  ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
