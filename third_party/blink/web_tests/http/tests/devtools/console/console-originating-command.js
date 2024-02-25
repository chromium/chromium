// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console result has originating command associated with it.\n`);
  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('1 + 1', step1);
  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages(true);
    TestRunner.completeTest();
  }
})();
