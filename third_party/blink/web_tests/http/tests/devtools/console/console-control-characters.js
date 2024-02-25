// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Verify that control characters are substituted with printable characters.\n`);

  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('var\u001d i = 0;', onEvaluated);

  async function onEvaluated() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
