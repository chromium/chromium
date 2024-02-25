// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests exception message contains stack with correct function name.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    var foo = function ()
    {
      throw new Error();
    }

    foo.displayName = 'foo.displayName';
    Object.defineProperty(foo, 'name', { value: 'foo.function.name' } );

    var bar = function()
    {
      foo();
    }

    bar.displayName = 'bar.displayName';

    var baz = function()
    {
      bar();
    }

    Object.defineProperty(baz, 'name', { value: 'baz.function.name' } );
  `);

  ConsoleTestRunner.waitUntilNthMessageReceived(1, step1);
  TestRunner.evaluateInPage('setTimeout(baz, 0);');

  function step1() {
    ConsoleTestRunner.expandConsoleMessages(step2);
  }

  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
