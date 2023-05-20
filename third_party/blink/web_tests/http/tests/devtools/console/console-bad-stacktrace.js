// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console messages with invalid stacktraces will still be rendered, crbug.com/826210\n');

  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('console');

  var consoleView = Console.ConsoleView.instance();
  consoleView.setImmediatelyFilterMessagesForTest();

  // Add invalid message.
  var badStackTrace = {
    callFrames: [
      {
        'functionName': '',
        'scriptId': 'invalid-ScriptId',
        'url': '',
        'lineNumber': 0,
        'columnNumber': 0,
      }
    ]
  };
  var badStackTraceMessage = new SDK.ConsoleMessage(
      TestRunner.runtimeModel,
      SDK.ConsoleMessage.FrontendMessageSource.ConsoleAPI,
      Protocol.Log.LogEntryLevel.Error, 'This should be visible', {
        type: Protocol.Runtime.ConsoleAPICalledEventType.Error,
        stackTrace: badStackTrace,
      });
  const consoleModel = SDK.targetManager.primaryPageTarget().model(SDK.ConsoleModel);
  consoleModel.addMessage(badStackTraceMessage);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
