// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console can filter messages by source.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('resources/log-source.js');
  await TestRunner.evaluateInPagePromise(`
    function log1()
    {
      console.log.apply(console, arguments);
    }

    for (var i = 0; i < 10; i++) {
      if (i % 2 == 0)
        log1(i + "topGroup"); // from console-filter-test.html
      else
        log2(i + "topGroup"); // from log-source.js
    }

    console.group("outerGroup");
    for (var i = 10; i < 20; i++) {
      if (i % 2 == 0)
        log1(i + "outerGroup"); // from console-filter-test.html
      else
        log2(i + "outerGroup"); // from log-source.js
    }
    console.group("innerGroup");
    for (var i = 20; i < 30; i++) {
      if (i % 2 == 0)
        log1(i + "innerGroup"); // from console-filter-test.html
      else
        log2(i + "innerGroup"); // from log-source.js
    }
    console.groupEnd();
    console.groupEnd();

    var logger1 = console.context("context1");
    var logger2 = console.context("context2");
    logger1.log("Hello 1");
    logger2.log("Hello 2");

    console.log("end");
  `);
  var consoleView = Console.ConsoleView.instance();
  consoleView._setImmediatelyFilterMessagesForTest();
  if (consoleView._isSidebarOpen)
    consoleView._splitWidget._showHideSidebarButton.element.click();

  // Add Violation-source message.
  var violationMessage = new SDK.ConsoleMessage(
      null, SDK.ConsoleMessage.MessageSource.Violation,
      SDK.ConsoleMessage.MessageLevel.Verbose,
      "Violation message text",
      SDK.ConsoleMessage.MessageType.Log);
  SDK.consoleModel.addMessage(violationMessage);

  var messages = Console.ConsoleView.instance()._visibleViewMessages;

  async function dumpVisibleMessages() {
    var messages = Console.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messages.length; ++i) {
      var viewMessage = messages[i];
      var delimeter = viewMessage.consoleMessage().isGroupStartMessage() ? '>' : '';
      var indent = '';
      for (var j = 0; j < viewMessage.nestingLevel(); ++j) indent += '  ';

      // Ordering is important here, as accessing the element the first time around
      // triggers live location creation and updates which we need to await properly.
      const element = viewMessage.element();
      await TestRunner.waitForPendingLiveLocationUpdates();
      TestRunner.addResult(indent + delimeter + element.deepTextContent());
    }
  }

  var url1 = messages[0].consoleMessage().url;
  var url2 = messages[1].consoleMessage().url;

  TestRunner.runTestSuite([
    function beforeFilter(next) {
      TestRunner.addResult('beforeFilter');
      dumpVisibleMessages().then(next);
    },
    function allLevelsFilter(next) {
      Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.allLevelsFilterValue());
      dumpVisibleMessages().then(next);
    },
    function addURL1Filter(next) {
      TestRunner.addResult('Blocking messages from ' + url1);
      Console.ConsoleView.instance()._filter.addMessageURLFilter(url1);
      dumpVisibleMessages().then(next);
    },
    function addURL2Filter(next) {
      TestRunner.addResult('Blocking messages from ' + url2);
      Console.ConsoleView.instance()._filter.addMessageURLFilter(url2);
      dumpVisibleMessages().then(next);
    },
    function removeAllFilters(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue('');
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkTextFilter(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue('outer');
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkMultiTextFilter(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("Group /[2-3]top/");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkTextUrlFilter(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("url:log-source");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkNegativeTextUrlFilter(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("-url:log-source");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkSourceFilter(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("source:violation");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkContextTextFilter(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("context:context");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkStartEndLineRegex(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("/^Hello\\s\\d$/");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkStartEndLineRegexForAnchor(next) {
      Console.ConsoleView.instance()._filter._textFilterUI.setValue("/^log-source\\.js:\\d+$/");
      Console.ConsoleView.instance()._filter._onFilterChanged();
      dumpVisibleMessages().then(next);
    },
    function checkResetFilter(next) {
      Console.ConsoleView.instance()._filter.reset();
      dumpVisibleMessages().then(next);
    }
  ]);
})();
