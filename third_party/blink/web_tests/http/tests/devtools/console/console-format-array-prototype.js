// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console logging dumps array values defined on Array.prototype[].\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function log(data)
    {
        console.log(data);
    }

    var a0 = [];
    var a1 = []; a1.length = 1;
    var a2 = []; a2.length = 5;
    var a3 = [,2,3];
    var a4 = []; a4.length = 15;
    var a5 = []; a5.length = 15; a5[8] = 8;
    var a6 = []; a6.length = 15; a6[0] = 0; a6[10] = 10;
    var a7 = [,,,4]; a7.length = 15;
    for (var i = 0; i < 6; ++i)
        a7["index" + i] = i;
    var a8 = [];
    for (var i = 0; i < 10; ++i)
        a8[i] = i;
    var a9 = [];
    for (var i = 1; i < 5; ++i) {
        a9[i] = i;
        a9[i + 5] = i + 5;
    }
    a9.length = 11;
    a9.foo = "bar";
    a10 = Object.create([1,2]);
    //# sourceURL=console-format-array-prototype.js
  `);

  loopOverGlobals(0, 11);

  function loopOverGlobals(current, total) {
    function advance() {
      var next = current + 1;

      if (next === total) {
        TestRunner.evaluateInPage('tearDown()');
        TestRunner.deprecatedRunAfterPendingDispatches(finish);
      } else {
        loopOverGlobals(next, total);
      }
    }

    async function finish() {
      await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
      TestRunner.completeTest();
    }

    ConsoleTestRunner.evaluateInConsole('a' + current);
    TestRunner.deprecatedRunAfterPendingDispatches(invokeConsoleLog);

    function invokeConsoleLog() {
      TestRunner.evaluateInPage('log(a' + current + ')');
      TestRunner.deprecatedRunAfterPendingDispatches(advance);
    }
  }
})();
