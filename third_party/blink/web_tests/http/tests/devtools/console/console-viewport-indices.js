// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as Console from 'devtools/panels/console/console.js';

(async function() {
    TestRunner.addResult(`Verifies viewport's visible and active message ranges.\n`);
    await TestRunner.showPanel('console');
    await TestRunner.evaluateInPagePromise(`
        function addNormalMessages(count)
        {
            for (var i = 0; i < count; ++i)
                console.log("Message #" + i);
        }

        function addMultilineMessages(count)
        {
            for (var i = 0; i < count; ++i)
                console.log("Message\\n#" + i);
        }

        function addSlightlyBiggerMessages(count)
        {
            for (var i = 0; i < count; ++i)
              console.log('%cMessage #' + i, 'padding-bottom: 1px');
        }

        //# sourceURL=console-viewport-indices.js
      `);

    ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
    var consoleView = Console.ConsoleView.ConsoleView.instance();
    var viewport = consoleView.viewport;

    function logMessages(count, type) {
      TestRunner.addResult(`Logging ${count} ${type} messages`);
      return new Promise(resolve => {
        var awaitingMessagesCount = count;
        function messageAdded() {
          if (!--awaitingMessagesCount) {
            viewport.invalidate();
            resolve();
          } else {
            ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
          }
        }
        ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
        TestRunner.evaluateInPage(Platform.StringUtilities.sprintf(`add${type}Messages(%d)`, count));
      });
    }

    function dumpVisibleIndices() {
      var {first, last, count} = ConsoleTestRunner.visibleIndices();
      var activeTotal = viewport.firstActiveIndex === -1 ? 0 : (viewport.lastActiveIndex - viewport.firstActiveIndex + 1);
      var calculatedFirst = viewport.firstVisibleIndex();
      var calculatedLast = viewport.lastVisibleIndex();
      var calculatedTotal = calculatedFirst === -1 ? 0 : (calculatedLast - calculatedFirst + 1);
      if (calculatedFirst !== first || calculatedLast !== last) {
        TestRunner.addResult('TEST ENDED IN ERROR: viewport is calculated incorrect visible indices!');
        TestRunner.addResult(`Calculated visible range: ${calculatedFirst} to ${calculatedLast}, Total: ${calculatedTotal}
Actual visible range: ${first} to ${last}, Total: ${count}`);
        TestRunner.completeTest();
      } else {
        TestRunner.addResult(`Expected and actual visible ranges match`);
      }
    }

    function forceItemAndDump(index, first) {
      TestRunner.addResult(`Force item to be ${first ? 'first' : 'last'}: ${index}`);
      if (first)
        viewport.forceScrollItemToBeFirst(index);
      else
        viewport.forceScrollItemToBeLast(index);
      dumpVisibleIndices();
    }

    TestRunner.runTestSuite([
      async function testEmptyViewport(next) {
        Console.ConsoleView.ConsoleView.clearConsole();
        dumpVisibleIndices();
        next();
      },

      async function testFirstLastVisibleIndices(next) {
        Console.ConsoleView.ConsoleView.clearConsole();
        await logMessages(100, 'Normal');

        forceItemAndDump(0, true);
        forceItemAndDump(1, true);

        var lessThanOneRowHeight = consoleView.minimumRowHeight() - 1;
        TestRunner.addResult(`Scroll a bit down: ${lessThanOneRowHeight}px`);
        viewport.element.scrollTop += lessThanOneRowHeight;
        viewport.refresh();
        dumpVisibleIndices();

        forceItemAndDump(50, false);
        forceItemAndDump(99, false);
        next();
      },

      async function testMultilineMessages(next) {
        Console.ConsoleView.ConsoleView.clearConsole();
        await logMessages(100, 'Multiline');

        forceItemAndDump(0, true);
        forceItemAndDump(1, true);

        var lessThanOneRowHeight = consoleView.minimumRowHeight() - 1;
        TestRunner.addResult(`Scroll a bit down: ${lessThanOneRowHeight}px`);
        viewport.element.scrollTop += lessThanOneRowHeight;
        viewport.refresh();
        dumpVisibleIndices();

        forceItemAndDump(50, false);
        forceItemAndDump(99, false);
        next();
      },

      async function testSlightlyBiggerMessages(next) {
        Console.ConsoleView.ConsoleView.clearConsole();
        await logMessages(100, 'SlightlyBigger');

        forceItemAndDump(0, true);
        forceItemAndDump(1, true);

        var lessThanOneRowHeight = consoleView.minimumRowHeight() - 1;
        TestRunner.addResult(`Scroll a bit down: ${lessThanOneRowHeight}px`);
        viewport.element.scrollTop += lessThanOneRowHeight;
        viewport.refresh();
        dumpVisibleIndices();

        forceItemAndDump(50, false);
        forceItemAndDump(99, false);
        next();
      }
    ]);
  })();
