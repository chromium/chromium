// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console sidebar behaves properly.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('resources/log-source.js');
  await TestRunner.evaluateInPagePromise(`
    function log1()
    {
      console.log.apply(console, arguments);
    }

    function error1()
    {
      console.error.apply(console, arguments);
    }
  `);

  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var sidebar = consoleView.sidebar;
  var messages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;
  consoleView.setImmediatelyFilterMessagesForTest();
  if (!consoleView.isSidebarOpen)
    consoleView.splitWidget.showHideSidebarButton.element.click();

  function dumpSidebar() {
    var treeElement = sidebar.tree.firstChild();
    var info = {};
    var depth = 1;
    TestRunner.addResult('SIDEBAR:');
    do {
      TestRunner.addResult('> '.repeat(depth) + treeElement.title);
      treeElement = treeElement.traverseNextTreeElement(false /* skipUnrevealed */, null, true /* dontPopulate */, info);
      depth += info.depthChange;
    } while (treeElement)
  }

  TestRunner.runTestSuite([
    function beforeFilter(next) {
      dumpSidebar();
      next();
    },
    async function addLogsFromMultipleUrls(next) {
      await TestRunner.evaluateInPagePromise(`log2('Log from log-source')`);
      await TestRunner.evaluateInPagePromise(`log1('Log from test')`);
      dumpSidebar();
      next();
    },
    async function addLogsFromMultipleLevels(next) {
      await TestRunner.evaluateInPagePromise(`warn2('Warning from log-source')`);
      await TestRunner.evaluateInPagePromise(`error1('Error from test')`);
      dumpSidebar();
      next();
    },
    async function selectingErrorGroup(next) {
      sidebar.treeElements[2].select();
      TestRunner.addResult('Selecting item: ' + sidebar.selectedTreeElement.title);
      TestRunner.addResult('MESSAGES:');
      await ConsoleTestRunner.dumpConsoleMessages();
      TestRunner.addResult('');
      dumpSidebar();
      next();
    },
    async function selectingFileGroup(next) {
      sidebar.treeElements[0].expand();
      sidebar.treeElements[0].select();
      sidebar.tree.selectNext();
      TestRunner.addResult('Selecting item: ' + sidebar.selectedTreeElement.title);
      TestRunner.addResult('MESSAGES:');
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    },
    async function clearConsole(next) {
      Console.ConsoleView.ConsoleView.clearConsole();
      dumpSidebar();
      next();
    }
  ]);
})();
