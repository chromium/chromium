// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Test that console.log can be called without console receiver.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    var log = console.log;
    log(1);
    var info = console.info;
    info(2);
    var error = console.error;
    error(3);
    var warn = console.warn;
    warn(4);
  `);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
