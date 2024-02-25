// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that Web Inspector's console is not broken if Object is overwritten in the inspected page. Test passes if the expression is evaluated in the console and no errors printed. Bug 101320.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    Object = function() {};
  `);

  ConsoleTestRunner.evaluateInConsole('var foo = {bar:2012}; foo', step1);

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
