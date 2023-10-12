// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`This test passes if only one deprecation warning is presented in the console.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    var x = window.webkitStorageInfo;
    var y = window.webkitStorageInfo;
  `);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
