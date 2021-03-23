// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console sidebar behaves properly.\n`);

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
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

  var consoleView = Console.ConsoleView.instance();
  var sidebar = consoleView._sidebar;
  var messages = Console.ConsoleView.instance()._visibleViewMessages;
  consoleView._setImmediatelyFilterMessagesForTest();
  if (!consoleView._isSidebarOpen)
    consoleView._splitWidget._showHideSidebarButton.element.click();

  function dumpSidebar() {
    var treeElement = sidebar._tree.firstChild();
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
      sidebar._treeElements[2].select();
      TestRunner.addResult('Selecting item: ' + sidebar._selectedTreeElement.title);
      TestRunner.addResult('MESSAGES:');
      await ConsoleTestRunner.dumpConsoleMessages();
      TestRunner.addResult('');
      dumpSidebar();
      next();
    },
    async function selectingFileGroup(next) {
      sidebar._treeElements[0].expand();
      sidebar._treeElements[0].select();
      sidebar._tree.selectNext();
      TestRunner.addResult('Selecting item: ' + sidebar._selectedTreeElement.title);
      TestRunner.addResult('MESSAGES:');
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    },
    async function clearConsole(next) {
      Console.ConsoleView.clearConsole();
      dumpSidebar();
      next();
    }
  ]);
})();
