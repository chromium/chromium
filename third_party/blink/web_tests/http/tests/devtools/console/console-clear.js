// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult("Tests that console is cleared upon requestClearMessages call.\n");

  await TestRunner.showPanel("console");

  await TestRunner.evaluateInPagePromise(`
    console.log("one");
    console.log("two");
    console.log("three");
  `);
  TestRunner.addResult("=== Before clear ===");
  await ConsoleTestRunner.dumpConsoleMessages();

  Console.ConsoleView.ConsoleView.clearConsole();
  TestRunner.deprecatedRunAfterPendingDispatches(callback);
  async function callback() {
    TestRunner.addResult("=== After clear ===");
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
