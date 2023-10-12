// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that Web Inspector won't crash if some console have been logged by the time it's opening.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    console.log('log');
    console.info('info');
    console.warn('warn');
    console.error('error');
  `);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
