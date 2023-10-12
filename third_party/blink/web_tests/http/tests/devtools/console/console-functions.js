// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console logging different types of functions correctly.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    var functions = [
        function simple() {},
        async function asyncSimple() {},
        function* genSimple() {},
        function(){},
        function(x, y){},
        function namedArgs(x) {},
        function namedArgs2(x, y) {},
        function ({}) {},
        function   *    whitespace   (  x  )    {   },
        async    function    whitespace2   (  x  ,  y  ,  z  )    {   },
    ];

    var obj = {};
    for (var i = 0; i < functions.length; ++i) {
        console.log(functions[i]);
        console.dir(functions[i]);
        obj["func" + i] = functions[i];
    }
    console.log(obj);
    console.dir(obj);
  `);

  ConsoleTestRunner.expandConsoleMessages(dumpConsoleMessages);

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
