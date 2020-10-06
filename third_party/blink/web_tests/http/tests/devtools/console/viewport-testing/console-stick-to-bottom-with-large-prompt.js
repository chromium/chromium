// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies viewport stick-to-bottom behavior when prompt has space below editable area.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function populateConsoleWithMessages(count)
      {
          for (var i = 0; i < count; ++i)
              console.log("Multiline\\nMessage #" + i);
      }

      //# sourceURL=console-viewport-stick-to-bottom-with-large-prompt.js
    `);
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  await ConsoleTestRunner.waitForPendingViewportUpdates();

  const consoleView = Console.ConsoleView.instance();
  const viewport = consoleView._viewport;
  const heightBelowPromptEditor = consoleView._prompt.belowEditorElement().offsetHeight;
  const messagesCount = 150;

  TestRunner.runTestSuite([
    async function testStickToBottomWhenAddingMessages(next) {
      await logMessagesToConsole(messagesCount);
      await ConsoleTestRunner.waitForPendingViewportUpdates();
      dumpAndContinue(next);
    },

    async function testScrollViewportToBottom(next) {
      viewport.element.scrollTop = 0;
      consoleView._immediatelyScrollToBottom();
      await ConsoleTestRunner.waitForPendingViewportUpdates();
      dumpAndContinue(next);
    },

    async function testJumpToBottomWhenTypingOnLastPromptLine(next) {
      viewport.element.scrollTop = 0;
      await ConsoleTestRunner.waitForPendingViewportUpdates();

      // Simulate scroll on type.
      viewport.element.scrollTop = viewport.element.scrollHeight - viewport.element.clientHeight - heightBelowPromptEditor;
      consoleView._prompt.setText('a');

      await ConsoleTestRunner.waitForPendingViewportUpdates();
      dumpAndContinue(next);
    },

    async function testDoNotJumpToBottomWhenTypingAboveLastPromptLine(next) {
      const multilineText = `Multiline text\n\n\nfoo`;
      consoleView._prompt.setText(multilineText);
      viewport.element.scrollTop = 0;
      await ConsoleTestRunner.waitForPendingViewportUpdates();

      // Simulate scroll on type.
      viewport.element.scrollTop = viewport.element.scrollHeight - viewport.element.clientHeight - heightBelowPromptEditor - 3;
      consoleView._prompt.setText(multilineText + 'a');

      await ConsoleTestRunner.waitForPendingViewportUpdates();
      dumpAndContinue(next);
    }
  ]);

  function dumpAndContinue(callback) {
    viewport.refresh();
    TestRunner.addResult(
      'Is at bottom: ' + TestRunner.isScrolledToBottom(viewport.element) + ', should stick: ' + viewport.stickToBottom());
    callback();
  }

  function logMessagesToConsole(count) {
    return new Promise(resolve => {
      var awaitingMessagesCount = count;
      function messageAdded() {
        if (!--awaitingMessagesCount)
          resolve();
        else
          ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
      }

      ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
      TestRunner.evaluateInPage(String.sprintf('populateConsoleWithMessages(%d)', count));
    })
  }
})();
