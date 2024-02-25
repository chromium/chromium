// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests saving nodes to temporary variables.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`<div id="node"></div>`);

  const node = await ElementsTestRunner.nodeWithIdPromise('node');
  ElementsTestRunner.firstElementsTreeOutline().saveNodeToTempVariable(node);
  const promise = TestRunner.addSnifferPromise(Console.ConsoleViewMessage.ConsoleViewMessage.prototype, 'formattedParameterAsNodeForTest');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  const secondMessage = Console.ConsoleView.ConsoleView.instance().visibleViewMessages[1];
  await promise;
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
