// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests how debugger presents special properties of closures, bound functions and object wrappers.`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      console.dir(Object(true));
      console.dir((function(a,b) { return a + b; }));
      console.dir((function(a,b) { return a + b; }).bind({}, 2));
      console.dir((function*() { yield [1,2,3] }));
  `);

  ConsoleTestRunner.expandConsoleMessages(dumpConsoleMessages);

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessages(
        false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
