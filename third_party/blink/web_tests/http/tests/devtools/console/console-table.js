// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
    `Tests that console.table is properly rendered on tables with more than 20 columns(maxColumnsToRender).\n`
  );

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    a = {};
    b = {};

    for (var i = 0; i < 15; i++)
      b["a" + i] = "a" + i;

    for (var i = 0; i < 15; i++) {
      a["b" + i] = "b" + i;
      b["b" + i] = "b" + i;
    }

    c = [a, b, a, b];
    d = [b, a, b, a];
  `);

  ConsoleTestRunner.disableConsoleViewport();
  ConsoleTestRunner.addConsoleViewSniffer(messageAdded, true);

  var count = 4;
  function messageAdded(message) {
    if (count === 2 || count === 3) ConsoleTestRunner.dumpConsoleTableMessage(message, true);

    if (--count === 0) TestRunner.completeTest();
  }

  ConsoleTestRunner.evaluateInConsole('console.table(c); console.table(d)');
})();
