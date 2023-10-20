// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console viewport handles selection properly.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function populateConsoleWithMessages(count)
      {
          for (var i = 0; i < count - 1; ++i)
              console.log("Message #" + i);
          console.log("hello %cworld", "color: blue");
      }
      //# sourceURL=console-viewport-selection.js
    `);

  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var viewport = consoleView.viewport;
  const minimumViewportMessagesCount = 10;
  const messagesCount = 150;
  const middleMessage = messagesCount / 2;
  var viewportMessagesCount;

  var testSuite = [
    async function testSelectionSingleLineText(next) {
      viewport.invalidate();
      viewport.forceScrollItemToBeFirst(0);
      viewportMessagesCount = viewport.lastVisibleIndex() - viewport.firstVisibleIndex() + 1;
      await selectMessages(middleMessage, 2, middleMessage, 7);
      dumpSelectionText();
      next();
    },

    async function testReversedSelectionSingleLineText(next) {
      await selectMessages(middleMessage, 7, middleMessage, 2);
      dumpSelectionText();
      next();
    },

    async function testSelectionMultiLineText(next) {
      await selectMessages(middleMessage - 1, 4, middleMessage + 1, 7);
      dumpSelectionText();
      next();
    },

    async function testSimpleVisibleSelection(next) {
      await selectMessages(middleMessage - 3, 6, middleMessage + 2, 6);
      dumpSelectionModel();
      next();
    },

    function testHalfScrollSelectionUp(next) {
      viewport.forceScrollItemToBeFirst(middleMessage);
      dumpSelectionModel();
      next();
    },

    function testHalfScrollSelectionDown(next) {
      viewport.forceScrollItemToBeLast(middleMessage);
      dumpSelectionModel();
      next();
    },

    function testScrollSelectionAwayUp(next) {
      viewport.forceScrollItemToBeFirst(0);
      dumpSelectionModel();
      next();
    },

    function testScrollSelectionAwayDown(next) {
      consoleView.immediatelyScrollToBottom();
      viewport.refresh();
      dumpSelectionModel();
      next();
    },

    async function testShiftClickSelectionOver(next) {
      await emulateShiftClickOnMessage(minimumViewportMessagesCount);
      dumpSelectionModel();
      next();
    },

    async function testShiftClickSelectionBelow(next) {
      await emulateShiftClickOnMessage(messagesCount - minimumViewportMessagesCount);
      dumpSelectionModel();
      next();
    },

    function testRemoveSelection(next) {
      var selection = window.getSelection();
      selection.removeAllRanges();
      dumpSelectionModel();
      next();
    },

    async function testReversedVisibleSelection(next) {
      await selectMessages(middleMessage + 1, 6, middleMessage - 4, 6);
      dumpSelectionModel();
      next();
    },

    async function testShiftClickReversedSelectionOver(next) {
      await emulateShiftClickOnMessage(minimumViewportMessagesCount);
      dumpSelectionModel();
      next();
    },

    async function testShiftClickReversedSelectionBelow(next) {
      await emulateShiftClickOnMessage(messagesCount - minimumViewportMessagesCount);
      dumpSelectionModel();
      next();
    },

    function testZeroOffsetSelection(next) {
      viewport.forceScrollItemToBeLast(messagesCount - 1);
      var lastMessageElement = viewport.renderedElementAt(messagesCount - 1);
      // there is a blue-colored "world" span in last message.
      var blueSpan = lastMessageElement;
      while (blueSpan.nodeName !== 'SPAN' || blueSpan.textContent !== 'world')
        blueSpan = blueSpan.traverseNextNode();

      window.getSelection().setBaseAndExtent(blueSpan, 0, blueSpan, blueSpan.childNodes.length);
      TestRunner.addResult('Selected text: ' + viewport.selectedText());
      next();
    },

    function testSelectAll(next) {
      viewport.forceScrollItemToBeFirst(0);

      // Set some initial selection in console.
      var base = consoleView.itemElement(messagesCount - 2).element();
      var extent = consoleView.itemElement(messagesCount - 1).element();
      window.getSelection().setBaseAndExtent(base, 0, extent, 0);

      // Try to select all messages.
      document.execCommand('selectAll');

      var text = viewport.selectedText();
      var count = text ? text.split('\n').length : 0;
      TestRunner.addResult(
          count === messagesCount ? 'Selected all ' + count + ' messages.' :
                                    'Selected ' + count + ' messages instead of ' + messagesCount);
      next();
    },

    function testSelectWithNonTextNodeContainer(next) {
      viewport.forceScrollItemToBeFirst(0);

      var nonTextNodeBase = consoleView.itemElement(1).element();
      var nonTextNodeExtent = consoleView.itemElement(2).element();
      var textNodeBase = consoleView.itemElement(1).element().traverseNextTextNode();
      var textNodeExtent = consoleView.itemElement(2).element().traverseNextTextNode();

      window.getSelection().setBaseAndExtent(nonTextNodeBase, 0, nonTextNodeExtent, 0);
      TestRunner.addResult('Selected text: ' + viewport.selectedText());

      window.getSelection().setBaseAndExtent(textNodeBase, 0, nonTextNodeExtent, 0);
      TestRunner.addResult('Selected text: ' + viewport.selectedText());

      window.getSelection().setBaseAndExtent(nonTextNodeBase, 0, textNodeExtent, 0);
      TestRunner.addResult('Selected text: ' + viewport.selectedText());

      next();
    }
  ];

  var awaitingMessagesCount = messagesCount;
  function messageAdded() {
    if (!--awaitingMessagesCount)
      TestRunner.runTestSuite(testSuite);
  }

  ConsoleTestRunner.addConsoleSniffer(messageAdded, true);
  TestRunner.evaluateInPage(Platform.StringUtilities.sprintf('populateConsoleWithMessages(%d)', messagesCount));

  function dumpSelectionModelElement(model) {
    if (!model)
      return 'null';
    return Platform.StringUtilities.sprintf('{item: %d, offset: %d}', model.item, model.offset);
  }

  function dumpSelectionModel() {
    viewport.refresh();
    var text = Platform.StringUtilities.sprintf(
        'anchor = %s, head = %s', dumpSelectionModelElement(viewport.anchorSelection),
        dumpSelectionModelElement(viewport.headSelection));
    TestRunner.addResult(text);
  }

  function dumpSelectionText() {
    viewport.refresh();
    var text = viewport.selectedText();
    TestRunner.addResult('Selected text:<<<EOL\n' + text + '\nEOL');
  }

  async function emulateShiftClickOnMessage(messageIndex) {
    viewport.refresh();
    var selection = window.getSelection();
    if (!selection || !selection.rangeCount) {
      TestRunner.addResult('FAILURE: There\'s no selection');
      return;
    }
    viewport.forceScrollItemToBeFirst(Math.max(messageIndex - minimumViewportMessagesCount / 2, 0));
    var element = consoleView.itemElement(messageIndex).element();
    // Console messages contain live locations.
    await TestRunner.waitForPendingLiveLocationUpdates();
    selection.setBaseAndExtent(selection.anchorNode, selection.anchorOffset, element, 0);
    viewport.refresh();
  }

  async function selectMessages(fromMessage, fromTextOffset, toMessage, toTextOffset) {
    if (Math.abs(toMessage - fromMessage) > minimumViewportMessagesCount) {
      TestRunner.addResult(Platform.StringUtilities.sprintf(
          'FAILURE: Cannot select more than %d messages (requested to select from %d to %d',
          minimumViewportMessagesCount, fromMessage, toMessage));
      TestRunner.completeTest();
      return;
    }
    viewport.forceScrollItemToBeFirst(Math.min(fromMessage, toMessage));

    await ConsoleTestRunner.selectConsoleMessages(fromMessage, fromTextOffset, toMessage, toTextOffset);
    viewport.refresh();
  }
})();
