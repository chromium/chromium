// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests the console timestamp setting.\n`);
  await TestRunner.showPanel('console');

  // It is essential that we calculate timezone for this particular moment of time
  // otherwise the time zone offset could be different because of DST.
  var baseDate = Date.parse('2014-05-13T16:53:20.123Z');
  var tzOffset = new Date(baseDate).getTimezoneOffset() * 60 * 1000;
  var baseTimestamp = 1400000000000 + tzOffset;

  Common.Settings.settingForTest('console-group-similar').set(false);

  function addMessageWithFixedTimestamp(messageText, timestamp, type) {
    var message = new SDK.ConsoleModel.ConsoleMessage(
        TestRunner.runtimeModel,
        Protocol.Log.LogEntrySource.Other,  // source
        Protocol.Log.LogEntryLevel.Info,    // level
        messageText, {
          type,
          // timestamp: 2014-05-13T16:53:20.123Z
          timestamp: timestamp || baseTimestamp + 123,
        });
    const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
    consoleModel.addMessage(message, true);  // allowGrouping
  }

  TestRunner.addResult('Console messages with timestamps disabled:');
  addMessageWithFixedTimestamp(
      '<Before> First Command', baseTimestamp + 789,
      SDK.ConsoleModel.FrontendMessageType.Command);
  addMessageWithFixedTimestamp(
      '<Before> First Result', baseTimestamp + 789,
      SDK.ConsoleModel.FrontendMessageType.Result);
  addMessageWithFixedTimestamp('<Before>');
  addMessageWithFixedTimestamp('<Before>', baseTimestamp + 456);
  addMessageWithFixedTimestamp('<Before>');
  addMessageWithFixedTimestamp(
      '<Before> Command', baseTimestamp,
      SDK.ConsoleModel.FrontendMessageType.Command);
  addMessageWithFixedTimestamp(
      '<Before> Result', baseTimestamp + 1,
      SDK.ConsoleModel.FrontendMessageType.Result);

  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.addResult('Console messages with timestamps enabled:');
  Common.Settings.settingForTest('console-timestamps-enabled').set(true);

  addMessageWithFixedTimestamp('<After>', baseTimestamp + 1000);
  addMessageWithFixedTimestamp('<After>', baseTimestamp + 1000);
  addMessageWithFixedTimestamp('<After>', baseTimestamp + 1456);

  Common.Settings.settingForTest('console-timestamps-enabled').set(false);
  Common.Settings.settingForTest('console-timestamps-enabled').set(true);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
