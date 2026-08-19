// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConsoleTestRunner} from 'console_test_runner';
import * as Console from 'devtools/panels/console/console.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that property values can be edited inline in the console via double click.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
     function logToConsole()
    {
        var obj = {a: 1, b: "foo", c: null, d: 2};
        console.log(obj);
    }
  `);

  ConsoleTestRunner.evaluateInConsole('logToConsole()', step1);

  function step1() {
    ConsoleTestRunner.expandConsoleMessages(step2);
  }

  async function step2() {
    var valueElements = await getValueElements();
    await doubleClickTypeAndEnter(valueElements[0], '1 + 2');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step3);
  }

  async function step3() {
    var valueElements = await getValueElements();
    await doubleClickTypeAndEnter(valueElements[1], 'nonExistingValue');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step4);
  }

  async function step4() {
    var valueElements = await getValueElements();
    await doubleClickTypeAndEnter(valueElements[2], '[1, 2, 3]');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step5);
  }

  async function step5() {
    var valueElements = await getValueElements();
    await doubleClickTypeAndEnter(valueElements[3], '{x: 2}');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step6);
  }

  async function step6() {
    await UI.Widget.Widget.allUpdatesComplete;
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }

  async function getValueElements() {
    await UI.Widget.Widget.allUpdatesComplete;
    var messageElement = Console.ConsoleView.ConsoleView.instance().visibleViewMessages[1].element();
    return messageElement.querySelector('.console-message-text devtools-tree')
        .shadowRoot.querySelectorAll('.value');
  }

  async function doubleClickTypeAndEnter(node, text) {
    var event = document.createEvent('MouseEvent');
    event.initMouseEvent('dblclick', true, true, null, 2);
    node.dispatchEvent(event);
    await UI.Widget.Widget.allUpdatesComplete;
    var messageElement = Console.ConsoleView.ConsoleView.instance().visibleViewMessages[1].element();
    var editPrompt =
        messageElement.querySelector('.console-message-text devtools-tree')
            .shadowRoot.querySelector('devtools-prompt[editing]')
            .shadowRoot.querySelector('.text-prompt');
    editPrompt.textContent = text;
    editPrompt.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    await UI.Widget.Widget.allUpdatesComplete;
  }
})();
