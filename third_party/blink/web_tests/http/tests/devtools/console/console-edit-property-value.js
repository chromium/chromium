// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

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

  function step2() {
    var valueElements = getValueElements();
    doubleClickTypeAndEnter(valueElements[0], '1 + 2');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step3);
  }

  function step3() {
    var valueElements = getValueElements();
    doubleClickTypeAndEnter(valueElements[1], 'nonExistingValue');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step4);
  }

  function step4() {
    var valueElements = getValueElements();
    doubleClickTypeAndEnter(valueElements[2], '[1, 2, 3]');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step5);
  }

  function step5() {
    var valueElements = getValueElements();
    doubleClickTypeAndEnter(valueElements[3], '{x: 2}');
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step6);
  }

  async function step6() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }

  function getValueElements() {
    var messageElement = Console.ConsoleView.ConsoleView.instance().visibleViewMessages[1].element();
    return messageElement.querySelector('.console-message-text *').shadowRoot.querySelectorAll('.value');
  }

  function doubleClickTypeAndEnter(node, text) {
    var event = document.createEvent('MouseEvent');
    event.initMouseEvent('dblclick', true, true, null, 2);
    node.dispatchEvent(event);
    TestRunner.addResult('Node was hidden after dblclick: ' + node.classList.contains('hidden'));
    var messageElement = Console.ConsoleView.ConsoleView.instance().visibleViewMessages[1].element();
    var editPrompt = messageElement.querySelector('.console-message-text *').shadowRoot.querySelector('.text-prompt');
    editPrompt.textContent = text;
    editPrompt.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }
})();
