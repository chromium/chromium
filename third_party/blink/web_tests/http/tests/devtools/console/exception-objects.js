// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that expressions have thrown objects.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function throwError()
      {
          throw new Error("error_text");
      }

      function throwObject()
      {
          throw {a: 42};
      }

      function throwNumber()
      {
          throw 42;
      }

      function rejectWithError()
      {
          Promise.reject(new Error("promise_error"));
      }

      function rejectWithObject()
      {
          Promise.reject({b: 42});
      }
      //# sourceURL=foo.js
    `);

  var expressions = [
    ['setTimeout(throwError, 0); undefined', 3], ['throwError();', 2], ['setTimeout(throwObject, 0); undefined', 3],
    ['throwObject();', 2], ['setTimeout(throwNumber, 0); undefined', 3], ['throwNumber();', 2],
    ['setTimeout(rejectWithError, 0); undefined', 3], ['rejectWithError();', 3],
    ['setTimeout(rejectWithObject, 0); undefined', 3], ['rejectWithObject();', 3]
  ];

  async function nextExpression() {
    if (!expressions.length) {
      await ConsoleTestRunner.dumpConsoleMessages();
      TestRunner.completeTest();
      return;
    }

    var expression = expressions.shift();
    ConsoleTestRunner.waitUntilNthMessageReceived(expression[1], nextExpression);
    ConsoleTestRunner.evaluateInConsole(expression[0], function() {});
  }

  nextExpression();
})();
