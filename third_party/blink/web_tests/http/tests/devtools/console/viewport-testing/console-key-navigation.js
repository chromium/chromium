// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console messages are navigable with the keyboard.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  const consoleView = Console.ConsoleView.instance();
  const viewport = consoleView._viewport;
  const prompt = consoleView._prompt;

  // Log some messages.
  const logCount = 100;
  await TestRunner.evaluateInPagePromise(`
      for (var i = 0; i < ${logCount}; ++i)
          console.log("Message #" + i);
    `);

  await ConsoleTestRunner.waitForConsoleMessagesPromise(logCount);
  await ConsoleTestRunner.waitForPendingViewportUpdates();

  TestRunner.runTestSuite([
    function testBetweenViewportAndExternal(next) {
      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      dumpFocus();

      shiftPress('Tab');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      shiftPress('Tab');
      dumpFocus();

      press('Tab');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      press('Tab');
      dumpFocus();

      next();
    },

    function testBetweenViewportAndExternalWithSelectedItemNotInDOM(next) {
      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      dumpFocus();

      shiftPress('Tab');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      scrollViewportToTop();
      dumpFocus();

      shiftPress('Tab');
      dumpFocus();

      press('Tab');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      TestRunner.addResult(`\nSetting focus in prompt:`);
      prompt.focus();
      dumpFocus();

      shiftPress('Tab');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      scrollViewportToTop();
      dumpFocus();

      press('Tab');
      dumpFocus();

      next();
    },

    function testMoveAcrossLogsWithinViewport(next) {
      forceSelect(logCount - 1);
      dumpFocus();

      press('Home');
      dumpFocus();

      press('ArrowDown');
      dumpFocus();

      press('ArrowDown');
      dumpFocus();

      press('End');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      press('ArrowUp');
      dumpFocus();

      next();
    },

    function testViewportDoesNotChangeFocusOnScroll(next) {
      forceSelect(logCount - 2);
      dumpFocus();

      scrollViewportToTop();
      dumpFocus();

      scrollViewportToBottom();
      dumpFocus();

      next();
    },

    function testViewportDoesNotStealFocusOnScroll(next) {
      forceSelect(logCount - 1);
      dumpFocus();

      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      dumpFocus();

      scrollViewportToTop();
      dumpFocus();

      scrollViewportToBottom();
      dumpFocus();

      next();
    },

    async function testNewLogsShouldNotMoveFocus(next) {
      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      dumpFocus();

      await TestRunner.evaluateInPagePromise(`console.log("New Message")`);
      await ConsoleTestRunner.waitForConsoleMessagesPromise(logCount + 1);
      await ConsoleTestRunner.waitForPendingViewportUpdates();

      dumpFocus();
      next();
    },

    function testClearingConsoleFocusesPrompt(next) {
      TestRunner.addResult(`\nConsole cleared:`);
      consoleView._consoleCleared();
      dumpFocus();
      next();
    }
  ]);


  // Utilities.
  function scrollViewportToTop() {
    TestRunner.addResult(`\nScrolling to top of viewport`);
    viewport.setStickToBottom(false);
    viewport.element.scrollTop = 0;
    viewport.refresh();
  }

  function scrollViewportToBottom() {
    TestRunner.addResult(`\nScrolling to bottom of viewport`);
    viewport.element.scrollTop = viewport.element.scrollHeight + 1;
    viewport.refresh();
  }

  function forceSelect(index) {
    TestRunner.addResult(`\nForce selecting index ${index}`);
    viewport._virtualSelectedIndex = index;
    viewport._contentElement.focus();
    viewport._updateFocusedItem();
  }

  function press(key) {
    TestRunner.addResult(`\n${key}:`);
    eventSender.keyDown(key);
  }

  function shiftPress(key) {
    TestRunner.addResult(`\nShift+${key}:`);
    eventSender.keyDown(key, ['shiftKey']);
  }

  function dumpFocus() {
    var element = document.deepActiveElement();
    if (!element) {
      TestRunner.addResult('null');
      return;
    }
    var name = element.tagName;
    if (element.id)
      name += '#' + element.id;
    if (element.getAttribute('aria-label'))
      name += ':' + element.getAttribute('aria-label');
    else if (element.title)
      name += ':' + element.title;
    else if (element.textContent && element.textContent.length < 50) {
      name += ':' + element.textContent.replace('\u200B', '');
    } else if (element.className)
      name += '.' + element.className.split(' ').join('.');
    TestRunner.addResult(name);
  }
})();
