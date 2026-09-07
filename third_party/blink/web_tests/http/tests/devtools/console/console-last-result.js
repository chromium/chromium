// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console exposes last evaluation result as $_.\n`);

  await TestRunner.showPanel('console');


  TestRunner.runTestSuite([
    async function testLastResult(next) {
      await ConsoleTestRunner.evaluateInConsolePromise('1+1');
      await ConsoleTestRunner.evaluateInConsolePromise('$_');
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    },
    async function testLastResultAfterConsoleClear(next) {
      await ConsoleTestRunner.evaluateInConsolePromise('1+1');
      Console.ConsoleView.ConsoleView.instance().clearConsole();
      await ConsoleTestRunner.waitForPendingViewportUpdates();
      await ConsoleTestRunner.evaluateInConsolePromise('$_');
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    }
  ]);
})();
