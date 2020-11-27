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
    async function testBetweenViewportAndExternal(next) {
      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      await dumpFocus();

      shiftPress('Tab');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      shiftPress('Tab');
      await dumpFocus();

      press('Tab');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      press('Tab');
      await dumpFocus();

      next();
    },

    async function testBetweenViewportAndExternalWithSelectedItemNotInDOM(next) {
      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      await dumpFocus();

      shiftPress('Tab');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      scrollViewportToTop();
      await dumpFocus();

      shiftPress('Tab');
      await dumpFocus();

      press('Tab');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      TestRunner.addResult(`\nSetting focus in prompt:`);
      prompt.focus();
      await dumpFocus();

      shiftPress('Tab');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      scrollViewportToTop();
      await dumpFocus();

      press('Tab');
      await dumpFocus();

      next();
    },

    async function testMoveAcrossLogsWithinViewport(next) {
      forceSelect(logCount - 1);
      await dumpFocus();

      press('Home');
      await dumpFocus();

      press('ArrowDown');
      await dumpFocus();

      press('ArrowDown');
      await dumpFocus();

      press('End');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      press('ArrowUp');
      await dumpFocus();

      next();
    },

    async function testViewportDoesNotChangeFocusOnScroll(next) {
      forceSelect(logCount - 2);
      await dumpFocus();

      scrollViewportToTop();
      await dumpFocus();

      scrollViewportToBottom();
      await dumpFocus();

      next();
    },

    async function testViewportDoesNotStealFocusOnScroll(next) {
      forceSelect(logCount - 1);
      await dumpFocus();

      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      await dumpFocus();

      scrollViewportToTop();
      await dumpFocus();

      scrollViewportToBottom();
      await dumpFocus();

      next();
    },

    async function testNewLogsShouldNotMoveFocus(next) {
      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      await dumpFocus();

      await TestRunner.evaluateInPagePromise(`console.log("New Message")`);
      await ConsoleTestRunner.waitForConsoleMessagesPromise(logCount + 1);
      await ConsoleTestRunner.waitForPendingViewportUpdates();

      await dumpFocus();
      next();
    },

    async function testClearingConsoleFocusesPrompt(next) {
      TestRunner.addResult(`\nConsole cleared:`);
      consoleView._consoleCleared();
      await dumpFocus();
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

  async function dumpFocus() {
    var element = document.deepActiveElement();
    // Console elements contain live locations that might not be fully resolved yet.
    await TestRunner.waitForPendingLiveLocationUpdates();
    if (!element) {
      TestRunner.addResult('null');
      return;
    }
    var name = element.tagName;
    if (element.id)
      name += '#' + element.id;
    if (element.getAttribute('aria-label'))
      name += ':' + element.getAttribute('aria-label');
    else if (UI.Tooltip.getContent(element))
      name += ':' + UI.Tooltip.getContent(element);
    else if (element.textContent && element.textContent.length < 50) {
      name += ':' + element.textContent.replace('\u200B', '');
    } else if (element.className)
      name += '.' + element.className.split(' ').join('.');
    TestRunner.addResult(name);
  }
})();
