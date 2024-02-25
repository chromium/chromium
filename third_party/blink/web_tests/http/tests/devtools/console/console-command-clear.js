// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console is cleared upon clear() eval in console.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
     function log() {
        // Fill console.
        console.log("one");
        console.log("two");
        console.log("three");
    }
    log();
  `);

  TestRunner.addResult('=== Before clear ===');
  await ConsoleTestRunner.dumpConsoleMessages();

  async function callback() {
    TestRunner.addResult('=== After clear ===');
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }

  ConsoleTestRunner.evaluateInConsole('clear()', callback);
})();
