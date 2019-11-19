// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console links are keyboard navigable.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  const consoleView = Console.ConsoleView.instance();
  const viewport = consoleView._viewport;
  const prompt = consoleView._prompt;

  await TestRunner.evaluateInPagePromise(`
    function fn1() {
      console.error("Custom error with link www.chromium.org/linkInErrMsg");
    }

    //# sourceURL=foo.js
  `);

  TestRunner.runTestSuite([
    async function testNavigatingLinks(next) {
      await clearAndLog(`console.log("Before");console.log("Text around www.chromium.org/1a multiple links, www.chromium.org/1b");console.log("www.chromium.org/2");`, 3);
      await ConsoleTestRunner.waitForRemoteObjectsConsoleMessagesPromise();

      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      shiftPress('Tab');

      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      next();
    },

    async function testNavigatingLinksInStackTrace(next) {
      await clearAndLog(`fn1()`, 1);
      await ConsoleTestRunner.waitForRemoteObjectsConsoleMessagesPromise();

      TestRunner.addResult(`Setting focus in prompt:`);
      prompt.focus();
      shiftPress('Tab');
      press('ArrowUp');  // Move from source link to message link.
      dumpFocus(true, 0, true);

      press('ArrowDown');

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowUp');
      dumpFocus(true, 0, true);

      press('ArrowRight');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowDown');
      dumpFocus(true, 0, true);

      press('ArrowLeft');
      dumpFocus(true, 0, true);

      press('ArrowLeft');
      dumpFocus(true, 0, true);

      next();
    },
  ]);


  // Utilities.
  async function clearAndLog(expression, expectedCount = 1) {
    consoleView._consoleCleared();
    TestRunner.addResult(`Evaluating: ${expression}`);
    await TestRunner.evaluateInPagePromise(expression);
    await ConsoleTestRunner.waitForConsoleMessagesPromise(expectedCount);
    await ConsoleTestRunner.waitForPendingViewportUpdates();
  }

  function press(key) {
    TestRunner.addResult(`\n${key}:`);
    eventSender.keyDown(key);
  }

  function shiftPress(key) {
    TestRunner.addResult(`\nShift+${key}:`);
    eventSender.keyDown(key, ['shiftKey']);
  }

  function dumpFocus(activeElement, messageIndex = 0, skipObjectCheck) {
    const firstMessage = consoleView._visibleViewMessages[messageIndex];
    const hasTrace = !!firstMessage.element().querySelector('.console-message-stack-trace-toggle');
    const hasHiddenStackTrace = firstMessage.element().querySelector('.console-message-stack-trace-wrapper > div.hidden');
    const hasCollapsedObject = firstMessage.element().querySelector('.console-view-object-properties-section:not(.expanded)');
    const hasExpandedObject = firstMessage.element().querySelector('.console-view-object-properties-section.expanded');

    TestRunner.addResult(`Viewport virtual selection: ${viewport._virtualSelectedIndex}`);

    if (!skipObjectCheck) {
      if (hasCollapsedObject) {
        TestRunner.addResult(`Has object: collapsed`);
      } else if (hasExpandedObject) {
        TestRunner.addResult(`Has object: expanded`);
      }
    }

    if (hasTrace) {
      TestRunner.addResult(`Is trace expanded: ${!hasHiddenStackTrace ? 'YES' : 'NO'}`);
    }
    if (firstMessage instanceof Console.ConsoleGroupViewMessage) {
      const expanded = !firstMessage.collapsed();
      TestRunner.addResult(`Is group expanded: ${expanded ? 'YES' : 'NO'}`);
    }

    if (!activeElement)
      return;
    var element = document.deepActiveElement();
    if (!element) {
      TestRunner.addResult('null');
      return;
    }
    var name = `activeElement: ${element.tagName}`;
    if (element.id)
      name += '#' + element.id;
    else if (element.className)
      name += '.' + element.className.split(' ').join('.');
    if (element.deepTextContent())
      name += '\nactive text: ' + element.deepTextContent();
    TestRunner.addResult(name);
  }
})();
