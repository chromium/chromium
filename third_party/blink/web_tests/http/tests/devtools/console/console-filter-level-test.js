// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console can filter messages by source.\n`);

  await TestRunner.loadModule('console_test_runner');
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
  var consoleView = Console.ConsoleView.instance();
  consoleView._setImmediatelyFilterMessagesForTest();
  if (consoleView._isSidebarOpen)
    consoleView._splitWidget._showHideSidebarButton.element.click();

  async function dumpVisibleMessages() {
    var menuText = Console.ConsoleView.instance()._filter._levelMenuButton._text;
    TestRunner.addResult('Level menu: ' + menuText);

    var messages = Console.ConsoleView.instance()._visibleViewMessages;
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
      TestRunner.addObject(Console.ConsoleFilter.allLevelsFilterValue());
      TestRunner.addResult('Default levels');
      TestRunner.addObject(Console.ConsoleFilter.defaultLevelsFilterValue());
      next();
    },

    function beforeFilter(next) {
      TestRunner.addResult('beforeFilter');
      dumpVisibleMessages().then(next);
    },

    function allLevels(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.allLevelsFilterValue());
      dumpVisibleMessages().then(next);
    },

    function defaultLevels(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.defaultLevelsFilterValue());
      dumpVisibleMessages().then(next);
    },

    function verbose(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set({ verbose: true });
      dumpVisibleMessages().then(next);
    },

    function info(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set({ info: true });
      dumpVisibleMessages().then(next);
    },

    function warningsAndErrors(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set({ warning: true, error: true });
      dumpVisibleMessages().then(next);
    },

    function abcMessagePlain(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set({ verbose: true });
      Console.ConsoleView.instance()._filter._textFilterUI.setValue('abc');
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },

    function abcMessageRegex(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue('/ab[a-z]/');
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },

    function abcMessageRegexWarning(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set({ warning: true });
      dumpVisibleMessages().then(next);
    }
  ];

  ConsoleTestRunner.evaluateInConsole(
    "'Should be always visible'",
    TestRunner.runTestSuite.bind(TestRunner, testSuite)
  );
})();
