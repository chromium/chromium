// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests exception message with empty stack in console contains async stack trace.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.DebuggerAgent.setAsyncCallStackDepth(200);

  ConsoleTestRunner.waitUntilNthMessageReceived(1, step2);
  TestRunner.evaluateInPage('setTimeout("~", 0)');

  function step2() {
    ConsoleTestRunner.expandConsoleMessages(step3);
  }

  async function step3() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
