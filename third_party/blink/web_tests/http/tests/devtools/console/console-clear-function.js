// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests that console is cleared via console.clear() method\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    function log()
    {
      // Fill console.
      console.log("one");
      console.log("two");
      console.log("three");
    }

    function clearConsoleFromPage()
    {
      console.clear();
    }
  `);

  TestRunner.runTestSuite([
    async function clearFromConsoleAPI(next) {
      await TestRunner.RuntimeAgent.evaluate('log();');
      TestRunner.addResult('=== Before clear ===');
      await ConsoleTestRunner.dumpConsoleMessages();

      await TestRunner.RuntimeAgent.evaluate('clearConsoleFromPage();');

      TestRunner.addResult('=== After clear ===');
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    },

    async function shouldNotClearWithPreserveLog(next) {
      await TestRunner.RuntimeAgent.evaluate('log();');
      TestRunner.addResult('=== Before clear ===');
      await ConsoleTestRunner.dumpConsoleMessages();
      Common.Settings.moduleSetting('preserve-console-log').set(true);

      await TestRunner.RuntimeAgent.evaluate('clearConsoleFromPage();');

      TestRunner.addResult('=== After clear ===');
      await ConsoleTestRunner.dumpConsoleMessages();
      Common.Settings.moduleSetting('preserve-console-log').set(false);
      next();
    }
  ]);
})();
