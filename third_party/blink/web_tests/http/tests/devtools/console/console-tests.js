// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console logging dumps proper messages.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    console.log('log');
    console.debug('debug');
    console.info('info');
    console.warn('warn');
    console.error('error');
    for (var i = 0; i < 5; ++i)
      console.log('repeated');
    for (var i = 0; i < 2; ++i)
      console.count('count');
    console.group('group');
    console.groupEnd();
    console.log('1', '2', '3');
    console.groupCollapsed('groupCollapsed');
    console.log({ property: "value" });
    console.log(42);
    console.log(true);
    console.log(null);
    console.log(undefined);
    console.log(document);
    console.log(function() { });
    console.log(function f() { });
    console.log([1, 2, 3]);
    console.log(/regexp.*/);
    console.groupEnd();
    console.count();
    console.count();
    console.count();
    console.count("title");
    console.count("title");
    console.count("title");
  `);

  Console.ConsoleView.ConsoleView.instance().setImmediatelyFilterMessagesForTest();
  Console.ConsoleView.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.ConsoleFilter.allLevelsFilterValue());
  await ConsoleTestRunner.dumpConsoleMessagesWithClasses();
  TestRunner.completeTest();
})();
