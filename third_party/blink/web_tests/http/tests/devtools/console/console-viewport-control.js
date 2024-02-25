// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Verifies viewport correctly shows and hides messages while logging and scrolling.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function addMessages(count)
      {
          for (var i = 0; i < count; ++i)
              console.log("Message #" + i);
      }

      function addRepeatingMessages(count)
      {
          for (var i = 0; i < count; ++i)
              console.log("Repeating message");
      }

      //# sourceURL=console-viewport-control.js
    `);

  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var viewport = consoleView.viewport;
  const smallCount = 3;

  // Measure visible/active ranges.
  await logMessages(100, false, '100');
  viewport.forceScrollItemToBeFirst(50);
  var {first, last, count} = ConsoleTestRunner.visibleIndices();
  var visibleCount = count;
  var maxActiveCount = viewport.lastActiveIndex - viewport.firstActiveIndex + 1;
  // Use this because # of active messages below visible area may be different
  // # of active messages above visible area.
  var minActiveCount = 2 * (first - viewport.firstActiveIndex - 1) + visibleCount;
  var activeCountAbove = first - viewport.firstActiveIndex;
  var activeCountBelow = viewport.lastActiveIndex - last;

  var wasAddedToDOM = new Set();
  var wasRemovedFromDOM = new Set();
  function onWasShown() {
    wasAddedToDOM.add(this);
  }
  function onWillHide() {
    wasRemovedFromDOM.add(this);
  }
  TestRunner.addSniffer(Console.ConsoleViewMessage.ConsoleViewMessage.prototype, 'wasShown', onWasShown, true);
  TestRunner.addSniffer(Console.ConsoleViewMessage.ConsoleViewMessage.prototype, 'willHide', onWillHide, true);

  function resetShowHideCounts() {
    wasAddedToDOM.clear();
    wasRemovedFromDOM.clear();
  }

  function logMessages(count, repeating, message) {
    TestRunner.addResult('Logging ' + message + ' messages');
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
      if (!repeating)
        TestRunner.evaluateInPage(Platform.StringUtilities.sprintf('addMessages(%d)', count));
      else
        TestRunner.evaluateInPage(Platform.StringUtilities.sprintf('addRepeatingMessages(%d)', count));
    });
  }

  function reset() {
    Console.ConsoleView.ConsoleView.clearConsole();
    resetShowHideCounts();
  }

  function assertDOMCount(expectMessage, count) {
    var actual = 0;
    for (var message of consoleView.visibleViewMessages) {
      if (message.elementInternal && message.elementInternal.isConnected)
        actual++;
    }
    var result = `Are there ${expectMessage} items in DOM? `;
    result += (actual === count ? 'true' : `FAIL: expected ${count}, actual ${actual}`);
    TestRunner.addResult(result);
  }

  function assertVisibleCount(expectMessage, count) {
    var actualVisible = ConsoleTestRunner.visibleIndices().count;
    var result = `Are there ${expectMessage} items visible? `;
    result += (actualVisible === count ? 'true' : `FAIL: expected ${count}, actualVisible ${actualVisible}`);
    TestRunner.addResult(result);
  }

  function assertSomeAddedRemoved(added, removed) {
    var actualAdded = wasAddedToDOM.size > 0;
    var actualRemoved = wasRemovedFromDOM.size > 0;

    var result = `Were ${added ? 'some' : 'no'} items added? `;
    result += (actualAdded === added ? 'true' : `FAIL: expected ${added}, actualAdded ${actualAdded}`);
    result += `\nWere ${removed ? 'some' : 'no'} items removed? `;
    result += (actualRemoved === removed ? 'true' : `FAIL: expected ${removed}, actualRemoved ${actualRemoved}`);
    TestRunner.addResult(result);
  }

  function assertCountAddedRemoved(messageAdded, added, mesasgeRemoved, removed) {
    var addedSize = wasAddedToDOM.size;
    var removedSize = wasRemovedFromDOM.size;

    var result = `Were ${messageAdded} items added? `;
    result += (addedSize === added ? 'true' : `FAIL: expected ${added}, addedSize ${addedSize}`);
    result += `\nWere ${mesasgeRemoved} items removed? `;
    result += (removedSize === removed ? 'true' : `FAIL: expected ${removed}, removedSize ${removedSize}`);
    TestRunner.addResult(result);
  }

  TestRunner.runTestSuite([
    async function addSmallCount(next) {
      reset();
      await logMessages(smallCount, false, 'smallCount');
      viewport.forceScrollItemToBeFirst(0);
      assertDOMCount('smallCount', smallCount);
      assertVisibleCount('smallCount', smallCount);
      next();
    },

    async function addMoreThanVisibleCount(next) {
      reset();
      await logMessages(visibleCount + 1, false, 'visibleCount + 1');
      viewport.forceScrollItemToBeFirst(0);
      assertDOMCount('visibleCount + 1', visibleCount + 1);
      assertVisibleCount('visibleCount', visibleCount);
      next();
    },

    async function addMaxActiveCount(next) {
      reset();
      await logMessages(maxActiveCount, false, 'maxActiveCount');
      viewport.forceScrollItemToBeFirst(0);
      assertDOMCount('maxActiveCount', maxActiveCount);
      assertVisibleCount('visibleCount', visibleCount);
      next();
    },

    async function addMoreThanMaxActiveCount(next) {
      reset();
      await logMessages(maxActiveCount + smallCount, false, 'maxActiveCount + smallCount');
      viewport.forceScrollItemToBeFirst(0);
      assertDOMCount('maxActiveCount', maxActiveCount);
      assertVisibleCount('visibleCount', visibleCount);
      next();
    },

    async function scrollToBottomInPartialActiveWindow(next) {
      reset();
      // Few enough messages so that they all fit in DOM.
      var visiblePlusHalfExtraRows = visibleCount + Math.floor((minActiveCount - visibleCount) / 2) - 1;
      await logMessages(visiblePlusHalfExtraRows, false, 'visiblePlusHalfExtraRows');
      viewport.forceScrollItemToBeFirst(0);
      resetShowHideCounts();
      // Set scrollTop above the bottom.
      const abovePrompt = viewport.element.scrollHeight - viewport.element.clientHeight - consoleView.prompt.belowEditorElement().offsetHeight - 3;
      viewport.element.scrollTop = abovePrompt;
      viewport.refresh();
      assertSomeAddedRemoved(false, false);
      assertDOMCount('visiblePlusHalfExtraRows', visiblePlusHalfExtraRows);
      next();
    },

    async function scrollToBottomInMoreThanActiveWindow(next) {
      reset();
      await logMessages(maxActiveCount + 1, false, 'maxActiveCount + 1');
      viewport.forceScrollItemToBeFirst(0);
      resetShowHideCounts();
      // Set scrollTop above the bottom.
      const abovePrompt = viewport.element.scrollHeight - viewport.element.clientHeight - consoleView.prompt.belowEditorElement().offsetHeight - 3;
      viewport.element.scrollTop = abovePrompt;
      viewport.refresh();
      assertSomeAddedRemoved(true, true);
      next();
    },

    async function shouldNotReconnectExistingElementsToDOM(next) {
      reset();
      await logMessages(smallCount, false, 'smallCount');
      await logMessages(smallCount, false, 'smallCount');
      assertCountAddedRemoved('smallCount * 2', smallCount * 2, '0', 0);
      next();
    },

    async function logRepeatingMessages(next) {
      reset();
      await logMessages(visibleCount, true, 'visibleCount');
      assertCountAddedRemoved('1', 1, '0', 0);
      assertDOMCount('1', 1);
      assertVisibleCount('1', 1);
      next();
    },

    async function reorderingMessages(next) {
      reset();
      await logMessages(smallCount, false, 'smallCount');
      resetShowHideCounts();
      TestRunner.addResult('Swapping messages 0 and 1');
      var temp = consoleView.visibleViewMessages[0];
      consoleView.visibleViewMessages[0] = consoleView.visibleViewMessages[1];
      consoleView.visibleViewMessages[1] = temp;
      viewport.invalidate();
      assertSomeAddedRemoved(false, false);
      next();
    }
  ]);
})();
