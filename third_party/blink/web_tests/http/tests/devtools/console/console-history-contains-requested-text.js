// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(
      `Tests that expression which is evaluated as Object Literal, is correctly stored in console history. (crbug.com/584881)\n`);

  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('{a:1, b:2}', step2);

  function step2() {
    var consoleView = Console.ConsoleView.ConsoleView.instance();
    TestRunner.addResult(consoleView.prompt.history().previous());
    TestRunner.completeTest();
  }
})();
