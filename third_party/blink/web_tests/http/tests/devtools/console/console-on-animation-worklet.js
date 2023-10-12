// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests console output from AnimationWorklet.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function importWorklet()
      {
          CSS.animationWorklet.import('resources/console-worklet-script.js');
      }
  `);

  ConsoleTestRunner.waitForConsoleMessages(4, finish);
  TestRunner.evaluateInPage('importWorklet();');

  async function finish() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
