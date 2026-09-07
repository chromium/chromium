// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Console from 'devtools/panels/console/console.js';
import * as Main from 'devtools/entrypoints/main/main.js';

(async function() {
  TestRunner.addResult(
      `Tests that XMLHttpRequest Logging works when Enabled and doesn't show logs when Disabled for asynchronous XHRs.\n`);

  function makeRequest() {
    return new Promise(resolve => NetworkTestRunner.makeSimpleXHR(
                           'GET', 'resources/xhr-exists.html', true, resolve));
  }

  Main.MainImpl.MainImpl.universeForTest.settings
      .settingForTest('monitoring-xhr-enabled')
      .set(true);
  let messagesPromise = ConsoleTestRunner.waitUntilNthMessageReceivedPromise(2);
  await makeRequest();
  await messagesPromise;
  TestRunner.addResult('XHR with logging enabled: ');
  // Sorting console messages to prevent flakiness.
  await ConsoleTestRunner.waitForPendingViewportUpdates();
  TestRunner.addResults(
      (await ConsoleTestRunner.dumpConsoleMessagesIntoArray()).sort());
  Console.ConsoleView.ConsoleView.instance().clearConsole();

  Main.MainImpl.MainImpl.universeForTest.settings
      .settingForTest('monitoring-xhr-enabled')
      .set(false);
  messagesPromise = ConsoleTestRunner.waitUntilMessageReceivedPromise();
  await makeRequest();
  await messagesPromise;
  TestRunner.addResult('XHR with logging disabled: ');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
