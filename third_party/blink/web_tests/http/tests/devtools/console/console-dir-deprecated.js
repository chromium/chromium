// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console does not log deprecated warning messages while dir-dumping objects.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.loadHTML(`
    <div id="foo"></div>
  `);
  await TestRunner.evaluateInPagePromise(`
     function logObjects()
    {
        console.dir(window);
        console.dir((document.createRange()).expand('document'));
    }
  `);

  TestRunner.evaluateInPage('logObjects()', step2);

  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
