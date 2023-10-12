// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that repeat count is properly updated.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`

    function dumpMessages()
    {
      for (var i = 0; i < 2; ++i)
        console.log("Message");

      for (var i = 0; i < 2; ++i)
        console.log(new Error("Message with error"));

      for (var i = 0; i < 2; ++i)
        console.error({a: 1});
    }

    function throwObjects() {
      for (var i = 0; i < 2; ++i)
        setTimeout(() => { throw {a: 1}; }, 0);
    }

    function throwPrimitiveValues() {
      for (var i = 0; i < 2; ++i)
        setTimeout(() => { throw "Primitive value"; }, 0);
    }

    var delayedResolver;
    var delayedPromise = new Promise(resolve => { delayedResolver = resolve; });

    //# sourceURL=console-repeat-count.js
  `);

  // Same Command multiple times with no immediate result.
  ConsoleTestRunner.evaluateInConsolePromise('await delayedPromise');
  ConsoleTestRunner.evaluateInConsolePromise('await delayedPromise');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);

  // Multiple Results with the same value.
  await TestRunner.evaluateInPagePromise(`delayedResolver()`);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(4);

  await TestRunner.evaluateInPagePromise('dumpMessages()');
  await TestRunner.evaluateInPagePromise('throwPrimitiveValues()');
  await TestRunner.evaluateInPagePromise('throwObjects()');

  await ConsoleTestRunner.waitForConsoleMessagesPromise(11);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
