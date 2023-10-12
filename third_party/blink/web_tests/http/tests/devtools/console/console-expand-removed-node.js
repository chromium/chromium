// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that removed Elements logged in the Console are properly formatted.\n`);
  await TestRunner.showPanel('console');

  TestRunner.addResult(`Adding element`);
  await TestRunner.evaluateInPagePromise(`
    var el = document.createElement('div');
    var child = document.createElement('span');
    el.appendChild(child);
    document.body.appendChild(el);
    undefined;
  `);
  const nodePromise = TestRunner.addSnifferPromise(Console.ConsoleViewMessage.ConsoleViewMessage.prototype, 'formattedParameterAsNodeForTest');
  TestRunner.evaluateInPagePromise(`console.log(el)`);
  await nodePromise;
  await ConsoleTestRunner.waitForPendingViewportUpdates();
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.addResult(`Removing element`);
  await TestRunner.evaluateInPagePromise(`el.remove()`);

  TestRunner.addResult(`Expanding element in Console`);
  await ConsoleTestRunner.expandConsoleMessagesPromise();
  await ConsoleTestRunner.waitForRemoteObjectsConsoleMessagesPromise();
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
