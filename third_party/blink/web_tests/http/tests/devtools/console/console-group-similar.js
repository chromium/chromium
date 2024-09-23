// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that console correctly groups similar messages.\n`);

  await TestRunner.showPanel('console');

  // Show all messages, including verbose.
  Console.ConsoleView.ConsoleView.instance().setImmediatelyFilterMessagesForTest();
  Console.ConsoleView.ConsoleView.instance().filter.textFilterUI.setValue("url:script");
  Console.ConsoleView.ConsoleView.instance().filter.messageLevelFiltersSetting.set(Console.ConsoleFilter.ConsoleFilter.allLevelsFilterValue());

  for (var i = 0; i < 5; i++) {
    // Groupable messages.
    addViolationMessage(
        'Verbose-level violation', `script${i}.js`,
        Protocol.Log.LogEntryLevel.Verbose);
    addViolationMessage(
        'Error-level violation', `script${i}.js`,
        Protocol.Log.LogEntryLevel.Error);
    addConsoleAPIMessage('ConsoleAPI log', `script${i}.js`);
    addViolationMessage(
        'Violation hidden by filter', `zzz.js`,
        Protocol.Log.LogEntryLevel.Verbose);

    // Non-groupable messages.
    await ConsoleTestRunner.evaluateInConsolePromise(`'evaluated command'`);
    await ConsoleTestRunner.evaluateInConsolePromise(`produce_reference_error`);
  }

  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.addResult('\n\nStop grouping messages:\n');
  Console.ConsoleView.ConsoleView.instance().groupSimilarSetting.set(false);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();

  /**
   * @param {string} text
   * @param {string} url
   * @param {string} level
   */
  function addViolationMessage(text, url, level) {
    var message = new SDK.ConsoleModel.ConsoleMessage(
        null, Protocol.Log.LogEntrySource.Violation, level, text,
        {type: Protocol.Runtime.ConsoleAPICalledEventType.Log, url});
    const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
    consoleModel.addMessage(message);
  }

  /**
   * @param {string} text
   * @param {string} url
   */
  function addConsoleAPIMessage(text,  url) {
    var message = new SDK.ConsoleModel.ConsoleMessage(
        null, Common.Console.FrontendMessageSource.ConsoleAPI,
        Protocol.Log.LogEntryLevel.Info, text,
        {type: Protocol.Runtime.ConsoleAPICalledEventType.Log, url});
    const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
    consoleModel.addMessage(message);
  }
})();
