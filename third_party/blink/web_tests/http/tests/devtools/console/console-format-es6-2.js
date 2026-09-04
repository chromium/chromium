// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console properly displays information about ES6 features.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    var globals = [];
    function log(current)
    {
        console.log(globals[current]);
        console.log([globals[current]]);
    }
    (function onload()
    {
        var map2 = new Map();
        map2.set(41, 42);
        map2.set({foo: 1}, {foo: 2});

        var iter1 = map2.values();
        iter1.next();

        var set2 = new Set();
        set2.add(41);
        set2.add({foo: 1});

        var iter2 = set2.keys();
        iter2.next();

        globals = [
            map2.keys(), map2.values(), map2.entries(),
            set2.keys(), set2.values(), set2.entries(),
            iter1, iter2,
        ];

    })();
  `);

  const total = await TestRunner.evaluateInPagePromise('globals.length');
  for (let current = 0; current < total; current++) {
    const targetCount = ConsoleTestRunner.consoleMessagesCount() + 2;
    const messagesPromise = new Promise(resolve => {
      ConsoleTestRunner.addConsoleViewSniffer(() => {
        if (ConsoleTestRunner.consoleMessagesCount() >= targetCount) {
          resolve();
        }
      }, true);
    });
    await TestRunner.evaluateInPagePromise('log(' + current + ')');
    await messagesPromise;
    await ConsoleTestRunner.evaluateInConsolePromise('globals[' + current +
                                                     ']');
  }

  await ConsoleTestRunner.dumpConsoleMessages(
      false, false, TestRunner.textContentWithLineBreaks);
  TestRunner.addResult('Expanded all messages');
  await ConsoleTestRunner.expandConsoleMessagesPromise();
  if (ConsoleTestRunner.waitForAllPopulations) {
    await ConsoleTestRunner.waitForAllPopulations();
  }
  await ConsoleTestRunner.dumpConsoleMessages(
      false, false, TestRunner.textContentWithLineBreaks);
  TestRunner.completeTest();
})();
