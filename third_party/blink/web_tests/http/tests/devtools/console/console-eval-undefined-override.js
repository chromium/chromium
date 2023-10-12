// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that evaluating something in console won't crash the browser if undefined value is overriden. The test passes if it doesn't crash. Bug 64155.\n`);

  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('var x = {a:1}; x.self = x; undefined = x;', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsole('unknownVar', step2);
  }

  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
