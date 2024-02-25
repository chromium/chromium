// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console logging dumps properly styled messages.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    (function onload()
    {
        console.log("%cBlue!.", "color: blue;");
        console.log("%cBlue! %cRed!", "color: blue;", "color: red;");
        console.log("%cBlue!\\n%cRed!", "color: blue;", "color: red;");
        console.log("%cwww.google.com", "color: blue");
        console.warn("%cwww.google.com", "color: blue; background: blue");
        console.error("%cwww.google.com", "color: blue; background: blue");
        console.log('\u001b[30m1\u001b[31m2\u001b[90m3\u001b[91m4\u001b[40m5\u001b[41m6\u001b[100m7\u001b[101m8');
        console.log('\u001b[30m%d\u001b[31m%f\u001b[90m%s\u001b[91m%d\u001b[40m%f\u001b[41m%s\u001b[100m%d\u001b[101m%f', 1, 1.1, 'a', 2, 2.2, 'b', 3, 3.3);
    })();
  `);

  ConsoleTestRunner.expandConsoleMessages(onExpanded);

  function onExpanded() {
    ConsoleTestRunner.dumpConsoleMessagesWithStyles();
    TestRunner.completeTest();
  }
})();
