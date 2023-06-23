// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console inputs are evaluated in REPL mode\n');

  await TestRunner.showPanel('console');

  TestRunner.addSniffer(TestRunner.RuntimeAgent, 'invoke_evaluate', function(args) {
    TestRunner.addResult('Called RuntimeAgent.invoke_evaluate');
    TestRunner.addResult("Value of 'replMode': " + args.replMode);
  });

  ConsoleTestRunner.evaluateInConsole('let a = 1; let a = 2;', step2);

  function step2() {
    TestRunner.completeTest();
  }
})();
