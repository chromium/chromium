// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests that the console can preserve log messages across cross-process navigations.`);
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('http://devtools.oopif.test:8000/devtools/console/resources/log-message.html')
  Common.Settings.settingForTest('preserve-console-log').set(true);
  await TestRunner.evaluateInPage(`logMessage('before navigation')`);
  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/console/resources/log-message.html')
  await TestRunner.evaluateInPage(`logMessage('after navigation')`);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
