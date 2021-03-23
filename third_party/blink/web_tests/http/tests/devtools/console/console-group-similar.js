// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console correctly groups similar messages.\n`);

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  // Show all messages, including verbose.
  Console.ConsoleView.instance()._setImmediatelyFilterMessagesForTest();
  Console.ConsoleView.instance()._filter._textFilterUI.setValue("url:script");
  Console.ConsoleView.instance()._filter._onFilterChanged();
  Console.ConsoleView.instance()._filter._currentFilter.levelsMask = Console.ConsoleFilter.allLevelsFilterValue();

  for (var i = 0; i < 5; i++) {
    // Groupable messages.
    addViolationMessage('Verbose-level violation', `script${i}.js`, SDK.ConsoleMessage.MessageLevel.Verbose);
    addViolationMessage('Error-level violation', `script${i}.js`, SDK.ConsoleMessage.MessageLevel.Error);
    addConsoleAPIMessage('ConsoleAPI log', `script${i}.js`);
    addViolationMessage('Violation hidden by filter', `zzz.js`, SDK.ConsoleMessage.MessageLevel.Verbose);

    // Non-groupable messages.
    await ConsoleTestRunner.evaluateInConsolePromise(`'evaluated command'`);
    await ConsoleTestRunner.evaluateInConsolePromise(`produce_reference_error`);
  }

  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.addResult('\n\nStop grouping messages:\n');
  Console.ConsoleView.instance()._groupSimilarSetting.set(false);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();

  /**
   * @param {string} text
   * @param {string} url
   * @param {string} level
   */
  function addViolationMessage(text, url, level) {
    var message = new SDK.ConsoleMessage(
        null, SDK.ConsoleMessage.MessageSource.Violation, level,
        text, SDK.ConsoleMessage.MessageType.Log, url);
    SDK.consoleModel.addMessage(message);
  }

  /**
   * @param {string} text
   * @param {string} url
   */
  function addConsoleAPIMessage(text,  url) {
    var message = new SDK.ConsoleMessage(
        null, SDK.ConsoleMessage.MessageSource.ConsoleAPI, SDK.ConsoleMessage.MessageLevel.Info,
        text, SDK.ConsoleMessage.MessageType.Log, url);
    SDK.consoleModel.addMessage(message);
  }
})();
