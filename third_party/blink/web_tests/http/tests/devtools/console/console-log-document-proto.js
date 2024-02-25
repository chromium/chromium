// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
    `Test that console.dir(document.__proto__) won't result in an exception when the message is formatted in the inspector.Bug 27169.\n`
  );

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    console.dir(document.__proto__);
  `);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
