// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult('Tests that console messages with invalid stacktraces will still be rendered, crbug.com/826210\n');

  await TestRunner.showPanel('console');

  var consoleView = Console.ConsoleView.ConsoleView.instance();
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
  var badStackTraceMessage = new SDK.ConsoleModel.ConsoleMessage(
      TestRunner.runtimeModel,
      Common.Console.FrontendMessageSource.ConsoleAPI,
      Protocol.Log.LogEntryLevel.Error, 'This should be visible', {
        type: Protocol.Runtime.ConsoleAPICalledEventType.Error,
        stackTrace: badStackTrace,
      });
  const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
  consoleModel.addMessage(badStackTraceMessage);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
