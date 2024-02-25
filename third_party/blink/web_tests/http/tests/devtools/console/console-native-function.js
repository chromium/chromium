// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console dumps native function without exception.\n`);
  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('Math.random', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsole('document.appendChild', step2);
  }

  function step2() {
    ConsoleTestRunner.expandConsoleMessages(onExpanded);
  }

  async function onExpanded() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
