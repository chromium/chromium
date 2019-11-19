// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies viewport stick-to-bottom behavior.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function populateConsoleWithMessages(count)
      {
          for (var i = 0; i < count; ++i)
              console.log("Multiline\\nMessage #" + i);
      }

      //# sourceURL=console-viewport-stick-to-bottom.js
    `);
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  var viewportHeight = 200;
  ConsoleTestRunner.fixConsoleViewportDimensions(600, viewportHeight);
  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView._viewport;
  const messagesCount = 150;

  logMessagesToConsole(messagesCount, async () => {
    await ConsoleTestRunner.waitForPendingViewportUpdates();
    TestRunner.runTestSuite(testSuite);
  });

  var testSuite = [
    function testScrollViewportToBottom(next) {
      consoleView._immediatelyScrollToBottom();
      dumpAndContinue(next);
    },

    function testConsoleSticksToBottom(next) {
      logMessagesToConsole(messagesCount, onMessagesDumped);

      async function onMessagesDumped() {
        await ConsoleTestRunner.waitForPendingViewportUpdates();
        dumpAndContinue(next);
      }
    },

    function testEscShouldNotJumpToBottom(next) {
      viewport.setStickToBottom(false);
      viewport.element.scrollTop -= 10;
      var keyEvent = TestRunner.createKeyEvent('Escape');
      viewport._contentElement.dispatchEvent(keyEvent);
      dumpAndContinue(next);
    },

    function testChangingPromptTextShouldRestickAtBottom(next) {
      TestRunner.addSniffer(Console.ConsoleView.prototype, '_promptTextChangedForTest', onContentChanged);
      // Since eventSender.keyDown() does not scroll prompt into view, simulate
      // behavior by setting a large scrollTop.
      consoleView._viewport.element.scrollTop = 1000000;
      var editorElement = consoleView._prompt.setText('a');

      function onContentChanged() {
        dumpAndContinue(next);
      }
    },

    function testViewportMutationsShouldPreserveStickToBottom(next) {
      viewport._contentElement.lastChild.innerText = 'More than 2 lines: foo\n\nbar';
      dumpAndContinue(onMessagesDumped);

      function onMessagesDumped() {
        viewport.setStickToBottom(false);
        viewport._contentElement.lastChild.innerText = 'More than 3 lines: foo\n\n\nbar';
        dumpAndContinue(next);
      }
    },

    function testMuteUpdatesWhileScrolling(next) {
      consoleView._updateStickToBottomOnPointerDown();
      viewport.element.scrollTop -= 10;

      TestRunner.addSniffer(Console.ConsoleView.prototype, '_scheduleViewportRefreshForTest', onMessageAdded);
      ConsoleTestRunner.evaluateInConsole('1 + 1');

      /**
       * @param {boolean} muted
       */
      function onMessageAdded(muted) {
        TestRunner.addResult('New messages were muted: ' + muted);
        TestRunner.addSniffer(
            Console.ConsoleView.prototype, '_scheduleViewportRefreshForTest', onMouseUpScheduledRefresh);
        TestRunner.addSniffer(Console.ConsoleView.prototype, '_updateViewportStickinessForTest', onUpdateStickiness);
        consoleView._updateStickToBottomOnPointerUp();
      }

      /**
       * @param {boolean} muted
       */
      function onMouseUpScheduledRefresh(muted) {
        TestRunner.addResult('Refresh was scheduled after dirty state');
      }

      function onUpdateStickiness() {
        next();
      }
    },

    function testShouldNotJumpToBottomWhenMultilinePromptIsBelowMessages(next) {
      // Set scrollTop above the bottom.
      viewport.element.scrollTop = viewport.element.scrollHeight - viewport.element.clientHeight - consoleView._prompt.belowEditorElement().offsetHeight - 3;
      consoleView._prompt.setText('Foo\n\nbar');

      dumpAndContinue(next);
    },

    function testShouldNotJumpToBottomWhenPromptFillsEntireViewport(next) {
      consoleView._prompt.setText('Foo' + '\n'.repeat(viewportHeight));

      // Set scrollTop above the bottom.
      viewport.element.scrollTop = viewport.element.scrollHeight - viewport.element.clientHeight - consoleView._prompt.belowEditorElement().offsetHeight - 3;

      // Trigger prompt text change.
      consoleView._prompt.setText('Bar' + '\n'.repeat(viewportHeight));

      dumpAndContinue(next);
    },

    async function testShouldStickWhenEnteringCommandAndPromptIsOutOfView(next) {
      consoleView._prompt.focus();
      consoleView._prompt.setText('1');

      // Set scrollTop such that prompt is not in visible area.
      viewport.setStickToBottom(false);
      viewport.element.scrollTop = 0;
      await ConsoleTestRunner.waitForPendingViewportUpdates();

      TestRunner.addResult(`Sending key: Enter`);
      eventSender.keyDown('Enter');
      await ConsoleTestRunner.waitForPendingViewportUpdates();

      dumpAndContinue(next);
    },
  ];

  function dumpAndContinue(callback) {
    viewport.refresh();
    TestRunner.addResult(
        'Is at bottom: ' + viewport.element.isScrolledToBottom() + ', should stick: ' + viewport.stickToBottom());
    callback();
  }

  function logMessagesToConsole(count, callback) {
    var awaitingMessagesCount = count;
    function messageAdded() {
      if (!--awaitingMessagesCount)
        callback();
      else
        ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
    }

    ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
    TestRunner.evaluateInPage(String.sprintf('populateConsoleWithMessages(%d)', count));
  }
})();
