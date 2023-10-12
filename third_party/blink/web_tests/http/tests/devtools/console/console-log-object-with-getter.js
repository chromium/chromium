// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console logging dumps object values defined by getters and allows to expand it.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    var obj = {}
    Object.defineProperty(obj, "foo", {enumerable: true, get: function() { return {a:1,b:2}; }});
    Object.defineProperty(obj, "bar", {enumerable: false, set: function(x) { this.baz = x; }});

    var arr = [];
    Object.defineProperty(arr, 0, {enumerable: true, get: function() { return 1; }});
    Object.defineProperty(arr, 1, {enumerable: false, set: function(x) { this.baz = x; }});

    var myError = new Error("myError");
    myError.stack = "custom stack";
    var objWithGetterExceptions = {
      get error() { throw myError },
      get string() { throw "myString" },
      get number() { throw 123 },
      get function() { throw function() {} },
    };

    function logObject()
    {
      console.log(obj);
      console.log(arr);
      console.log(objWithGetterExceptions);
    }
  `);

  TestRunner.evaluateInPage('logObject()', step2);
  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessages();
    step3();
  }
  function step3() {
    ConsoleTestRunner.expandConsoleMessages(step4);
  }
  function step4() {
    ConsoleTestRunner.expandGettersInConsoleMessages(step5);
  }
  async function step5() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
