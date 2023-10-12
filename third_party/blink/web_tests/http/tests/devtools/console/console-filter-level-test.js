// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console can filter messages by source.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('resources/log-source.js');
  await TestRunner.evaluateInPagePromise(`
    console.info("sample info");
    console.log("sample log");
    console.warn("sample warning");
    console.debug("sample debug");
    console.error("sample error");

    console.info("abc info");
    console.info("def info");

    console.warn("abc warn");
    console.warn("def warn");
  `);
  var consoleView = Console.ConsoleView.ConsoleView.instance();
  consoleView.setImmediatelyFilterMessagesForTest();
  if (consoleView.isSidebarOpen)
    consoleView.splitWidget.showHideSidebarButton.element.click();

  async function dumpVisibleMessages() {
    var menuText = Console.ConsoleView.ConsoleView.instance().filter.levelMenuButton.text;
    TestRunner.addResult('Level menu: ' + menuText);

    var messages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;
    for (var i = 0; i < messages.length; i++) {
      // Ordering is important here, as accessing the element the first time around
      // triggers live location creation and updates which we need to await properly.
      const element = messages[i].element();
      await TestRunner.waitForPendingLiveLocationUpdates();
      TestRunner.addResult('>' + element.deepTextContent());
    }
  }

  var testSuite = [
    function dumpLevels(next) {
      TestRunner.addResult('All levels');
      TestRunner.addObject(Console.ConsoleFilter.ConsoleFilter.allLevelsFilterValue());
      TestRunner.addResult('Default levels');
      TestRunner.addObject(Console.ConsoleFilter.ConsoleFilter.defaultLevelsFilterValue());
      next();
    },

    function beforeFilter(next) {
      TestRunner.addResult('beforeFilter');
      dumpVisibleMessages().then(next);
    },

    function allLevels(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.ConsoleFilter.allLevelsFilterValue());
      dumpVisibleMessages().then(next);
    },

    function defaultLevels(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.ConsoleFilter.defaultLevelsFilterValue());
      dumpVisibleMessages().then(next);
    },

    function verbose(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set({ verbose: true });
      dumpVisibleMessages().then(next);
    },

    function info(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set({ info: true });
      dumpVisibleMessages().then(next);
    },

    function warningsAndErrors(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set({ warning: true, error: true });
      dumpVisibleMessages().then(next);
    },

    function abcMessagePlain(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set({ verbose: true });
      Console.ConsoleView.ConsoleView.instance().filter.textFilterUI.setValue('abc');
      Console.ConsoleView.ConsoleView.instance().filter.onFilterChanged();
      dumpVisibleMessages().then(next);
    },

    function abcMessageRegex(next) {
      Console.ConsoleView.ConsoleView.instance().filter.textFilterUI.setValue('/ab[a-z]/');
      Console.ConsoleView.ConsoleView.instance().filter.onFilterChanged();
      dumpVisibleMessages().then(next);
    },

    function abcMessageRegexWarning(next) {
      Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set({ warning: true });
      dumpVisibleMessages().then(next);
    }
  ];

  ConsoleTestRunner.evaluateInConsole(
    "'Should be always visible'",
    TestRunner.runTestSuite.bind(TestRunner, testSuite)
  );
})();
