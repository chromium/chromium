// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console messages are navigable with the keyboard.\n`);
  await TestRunner.showPanel('console');
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  const consoleView = Console.ConsoleView.ConsoleView.instance();
  const viewport = consoleView.viewport;

  TestRunner.runTestSuite([
    async function testClickOnLog(next) {
      await clearAndLog(`console.log(1)`);
      TestRunner.addResult(`Click on message`);
      clickAndFocus(consoleView.visibleViewMessages[0].element());

      dumpFocus();

      next();
    },

    async function testClickOnGroup(next) {
      await clearAndLog(`console.group('group1')`);
      TestRunner.addResult(`Click on message`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-message'));

      dumpFocus();

      next();
    },

    async function testClickOnTrace(next) {
      await clearAndLog(`console.warn('warning1')`);
      TestRunner.addResult(`Click on message`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-message-stack-trace-wrapper > div'));

      dumpFocus();

      next();
    },

    async function testClickOnObject(next) {
      await clearAndLog(`console.log({x: 1})`);
      TestRunner.addResult(`Click on object`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-object'));


      dumpFocus();

      next();
    },

    async function testClickOnTraceWithObject(next) {
      await clearAndLog(`console.warn('warn', {x: 1})`);
      TestRunner.addResult(`Click on object`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-object'));
      dumpFocus();

      resetFocusAndSelection();
      TestRunner.addResult(`Click on trace`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-message-stack-trace-wrapper > div'));

      dumpFocus();

      next();
    },

    async function testClickOnGroupWithObject(next) {
      await clearAndLog(`console.group('group', {x: 1})`);
      TestRunner.addResult(`Click on object`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-object'));
      dumpFocus();

      resetFocusAndSelection();
      TestRunner.addResult(`Click on group`);
      clickAndFocus(consoleView.visibleViewMessages[0].element().querySelector('.console-message'));

      dumpFocus();

      next();
    },
  ]);

  function clickAndFocus(element) {
    element.focus();
    element.click();
  }

  function resetFocusAndSelection() {
    viewport.virtualSelectedIndex = -1;
    consoleView.prompt.focus();
  }

  async function clearAndLog(expression) {
    consoleView.consoleCleared();
    TestRunner.addResult(`Evaluating: ${expression}`);
    await TestRunner.evaluateInPagePromise(expression);
    await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
    await ConsoleTestRunner.waitForPendingViewportUpdates();
  }

  function dumpFocus() {
    const firstMessage = consoleView.visibleViewMessages[0];
    const hasTrace = !!firstMessage.element().querySelector('.console-message-stack-trace-toggle .console-message-expand-icon');
    const hasHiddenStackTrace = firstMessage.element().querySelector('.console-message-stack-trace-wrapper > div.hidden-stack-trace');
    const hasCollapsedObject = firstMessage.element().querySelector('.console-view-object-properties-section.hidden');
    const hasExpandedObject = firstMessage.element().querySelector('.console-view-object-properties-section:not(.hidden)');

    TestRunner.addResult(`Viewport virtual selection: ${viewport.virtualSelectedIndex}`);

    if (hasCollapsedObject) {
      TestRunner.addResult(`Has object: collapsed`);
    } else if (hasExpandedObject) {
      TestRunner.addResult(`Has object: expanded`);
    }

    if (hasTrace) {
      TestRunner.addResult(`Is trace expanded: ${!hasHiddenStackTrace ? 'YES' : 'NO'}`);
    }
    if (firstMessage instanceof Console.ConsoleViewMessage.ConsoleGroupViewMessage) {
      const expanded = !firstMessage.collapsed();
      TestRunner.addResult(`Is group expanded: ${expanded ? 'YES' : 'NO'}`);
    }

    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (!element) {
      TestRunner.addResult('null');
      return;
    }
    var name = `activeElement: ${element.tagName}`;
    if (element.id)
      name += '#' + element.id;
    if (element.getAttribute('aria-label'))
      name += ':' + element.getAttribute('aria-label');
    if (element.title)
      name += ':' + element.title;
    if (element.className)
      name += '.' + element.className.split(' ').join('.');
    TestRunner.addResult(name);
  }
})();
