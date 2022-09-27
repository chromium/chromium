// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the console timestamp setting.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  // It is essential that we calculate timezone for this particular moment of time
  // otherwise the time zone offset could be different because of DST.
  var baseDate = Date.parse('2014-05-13T16:53:20.123Z');
  var tzOffset = new Date(baseDate).getTimezoneOffset() * 60 * 1000;
  var baseTimestamp = 1400000000000 + tzOffset;

  Common.settingForTest('consoleGroupSimilar').set(false);

  function addMessageWithFixedTimestamp(messageText, timestamp, type) {
    var message = new SDK.ConsoleMessage(
        TestRunner.runtimeModel,
        Protocol.Log.LogEntrySource.Other,  // source
        Protocol.Log.LogEntryLevel.Info,    // level
        messageText, {
          type,
          // timestamp: 2014-05-13T16:53:20.123Z
          timestamp: timestamp || baseTimestamp + 123,
        });
    SDK.consoleModel.addMessage(message, true);  // allowGrouping
  }

  TestRunner.addResult('Console messages with timestamps disabled:');
  addMessageWithFixedTimestamp(
      '<Before> First Command', baseTimestamp + 789,
      SDK.ConsoleMessage.FrontendMessageType.Command);
  addMessageWithFixedTimestamp(
      '<Before> First Result', baseTimestamp + 789,
      SDK.ConsoleMessage.FrontendMessageType.Result);
  addMessageWithFixedTimestamp('<Before>');
  addMessageWithFixedTimestamp('<Before>', baseTimestamp + 456);
  addMessageWithFixedTimestamp('<Before>');
  addMessageWithFixedTimestamp(
      '<Before> Command', baseTimestamp,
      SDK.ConsoleMessage.FrontendMessageType.Command);
  addMessageWithFixedTimestamp(
      '<Before> Result', baseTimestamp + 1,
      SDK.ConsoleMessage.FrontendMessageType.Result);

  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.addResult('Console messages with timestamps enabled:');
  Common.settingForTest('consoleTimestampsEnabled').set(true);

  addMessageWithFixedTimestamp('<After>', baseTimestamp + 1000);
  addMessageWithFixedTimestamp('<After>', baseTimestamp + 1000);
  addMessageWithFixedTimestamp('<After>', baseTimestamp + 1456);

  Common.settingForTest('consoleTimestampsEnabled').set(false);
  Common.settingForTest('consoleTimestampsEnabled').set(true);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
