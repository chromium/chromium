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
    var object = {
        foo(a,b) {
            console.log(42);
        },
        boo: function(c,d) {
            console.log(42);
        },
        get baz() {},
        set baz(x) {},
        * gen() {}
    }
    console.log(object);
  `);

  ConsoleTestRunner.expandConsoleMessages(step1);

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
