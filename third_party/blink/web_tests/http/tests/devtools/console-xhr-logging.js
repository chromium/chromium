// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  // This await is necessary for evaluateInPagePromise to produce accurate line numbers.
  await TestRunner.addResult(`Tests that XMLHttpRequest Logging works when Enabled and doesn't show logs when Disabled.\n`);
  await TestRunner.evaluateInPagePromise(`
      function requestHelper(method, url)
      {
          // Make synchronous requests for simplicity.
          console.log("sending a %s request to %s", method, url);
          makeSimpleXHR(method, url, false);
      }
  `);
  Common.Settings.settingForTest('console-group-similar').set(false);
  Common.Settings.settingForTest('monitoring-xhr-enabled').set(true);

  TestRunner.evaluateInPage(`requestHelper('GET', 'resources/xhr-exists.html')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.evaluateInPage(`requestHelper('GET', 'resources/xhr-does-not-exist.html')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(3);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.evaluateInPageAsync(`requestHelper('POST', 'resources/post-target.cgi')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.evaluateInPageAsync(`requestHelper('GET', 'http://localhost:8000/devtools/resources/cors-disabled/xhr-exists.html')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(4);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  Common.Settings.settingForTest('monitoring-xhr-enabled').set(false);

  TestRunner.evaluateInPageAsync(`requestHelper('GET', 'resources/xhr-exists.html')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.evaluateInPageAsync(`requestHelper('GET', 'resources/xhr-does-not-exist.html')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.evaluateInPageAsync(`requestHelper('POST', 'resources/post-target.cgi')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.evaluateInPageAsync(`requestHelper('GET', 'http://localhost:8000/devtools/resources/cors-disabled/xhr-exists.html')`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(3);
  await dumpConsoleMessagesSorted();
  SDK.ConsoleModel.ConsoleModel.requestClearMessages();
  TestRunner.addResult('');

  TestRunner.deprecatedRunAfterPendingDispatches(async () => {
    await dumpConsoleMessagesSorted();
    TestRunner.completeTest();
  });

  async function dumpConsoleMessagesSorted() {
    const messages = await ConsoleTestRunner.dumpConsoleMessagesIntoArray(false, false, ConsoleTestRunner.prepareConsoleMessageTextTrimmed);
    messages.sort().forEach(TestRunner.addResult);
  };

})();
