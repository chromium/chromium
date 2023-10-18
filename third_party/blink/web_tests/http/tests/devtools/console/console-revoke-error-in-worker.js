// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that console revokes lazily handled promise rejections.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      var worker;

      function createPromise()
      {
          worker = new Worker("resources/worker-with-defer-handled-promise.js");
      }

      function handlePromiseRejection()
      {
          worker.postMessage("");
      }
  `);

  SDK.TargetManager.TargetManager.instance().addModelListener(SDK.ConsoleModel.ConsoleModel, SDK.ConsoleModel.Events.MessageAdded, messageAdded);
  SDK.TargetManager.TargetManager.instance().addModelListener(SDK.ConsoleModel.ConsoleModel, SDK.ConsoleModel.Events.MessageUpdated, messageUpdated);

  Console.ConsoleView.ConsoleView.instance().setImmediatelyFilterMessagesForTest();
  TestRunner.addResult('Creating worker with promise');
  TestRunner.evaluateInPageWithTimeout('createPromise()');

  async function messageAdded(event) {
    TestRunner.addResult('');
    TestRunner.addResult('Message added: ' + event.data.level + ' ' + event.data.type);

    if (event.data.level === Protocol.Log.LogEntryLevel.Error) {
      await ConsoleTestRunner.dumpConsoleCounters();
      TestRunner.addResult('');
      TestRunner.addResult('Handling promise');
      TestRunner.evaluateInPageWithTimeout('handlePromiseRejection()');
    }
  }

  async function messageUpdated() {
    await ConsoleTestRunner.dumpConsoleCounters();

    // Turn on verbose filter.
    TestRunner.addResult(`\nEnable verbose filter`);
    Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.ConsoleFilter.allLevelsFilterValue());
    await ConsoleTestRunner.dumpConsoleCounters();

    TestRunner.completeTest();
  }
})();
