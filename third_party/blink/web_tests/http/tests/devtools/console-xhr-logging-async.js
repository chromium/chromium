// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(
      `Tests that XMLHttpRequest Logging works when Enabled and doesn't show logs when Disabled for asynchronous XHRs.\n`);
  await TestRunner.loadLegacyModule('console');

  step1();

  function makeRequest(callback) {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/xhr-exists.html', true, callback);
  }

  function step1() {
    Common.Settings.settingForTest('monitoringXHREnabled').set(true);
    makeRequest(() => {
      TestRunner.deprecatedRunAfterPendingDispatches(async () => {
        TestRunner.addResult('XHR with logging enabled: ');
        // Sorting console messages to prevent flakiness.
        await ConsoleTestRunner.waitForPendingViewportUpdates();
        TestRunner.addResults((await ConsoleTestRunner.dumpConsoleMessagesIntoArray()).sort());
        Console.ConsoleView.clearConsole();
        step2();
      });
    });
  }

  function step2() {
    Common.Settings.settingForTest('monitoringXHREnabled').set(false);
    makeRequest(() => {
      TestRunner.deprecatedRunAfterPendingDispatches(async () => {
        TestRunner.addResult('XHR with logging disabled: ');
        await ConsoleTestRunner.dumpConsoleMessages();
        TestRunner.completeTest();
      });
    });
  }
})();
