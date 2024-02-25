// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that the console can preserve log messages across navigations. Bug 53359\n`);
  await TestRunner.showPanel('console');

  const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
  consoleModel.addMessage(new SDK.ConsoleModel.ConsoleMessage(
      TestRunner.runtimeModel, Protocol.Log.LogEntrySource.Other,
      Protocol.Log.LogEntryLevel.Info, 'PASS'));
  Common.Settings.settingForTest('preserve-console-log').set(true);
  TestRunner.reloadPage(async function() {
    await ConsoleTestRunner.dumpConsoleMessages();
    Common.Settings.settingForTest('preserve-console-log').set(false);
    TestRunner.completeTest();
  });
})();
