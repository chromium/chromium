// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that console logging large messages will be truncated.\n');

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  const consoleView = Console.ConsoleView.instance();
  const maxLength = 40;
  Console.ConsoleViewMessage.setMaxTokenizableStringLength(maxLength);
  ObjectUI.ObjectPropertiesSection._maxRenderableStringLength = maxLength;
  const visibleLength = 20;
  Console.ConsoleViewMessage.setLongStringVisibleLength(visibleLength);
  const overMaxLength = maxLength * 2;
  TestRunner.addResult(`Setting max length to: ${maxLength}`);
  TestRunner.addResult(`Setting long string visible length to: ${visibleLength}`);

  await ConsoleTestRunner.evaluateInConsolePromise(`"a".repeat(${overMaxLength})`);
  await TestRunner.evaluateInPagePromise(`console.log("a".repeat(${overMaxLength}))`);
  await TestRunner.evaluateInPagePromise(`console.log("a".repeat(${overMaxLength}), "b".repeat(${overMaxLength}))`);
  await TestRunner.evaluateInPagePromise(`console.log("%s", "a".repeat(${overMaxLength}))`);
  await TestRunner.evaluateInPagePromise(`console.log("%o", "a".repeat(${overMaxLength}))`);
  await TestRunner.evaluateInPagePromise(`console.log("%c" + "a".repeat(${overMaxLength}), "color: green")`);
  await TestRunner.evaluateInPagePromise(`console.log("foo %s %o bar", "a".repeat(${overMaxLength}), {a: 1})`);
  await TestRunner.evaluateInPagePromise(`console.log({a: 1}, "a".repeat(${overMaxLength}), {b: 1})`);
  await TestRunner.evaluateInPagePromise(`console.log("a".repeat(${overMaxLength}), "https://chromium.org")`);
  await TestRunner.evaluateInPagePromise(`console.log("https://chromium.org", "a".repeat(${overMaxLength}))`);
  await TestRunner.evaluateInPagePromise(`console.log(RegExp("a".repeat(${overMaxLength})))`);
  await TestRunner.evaluateInPagePromise(`console.log(Symbol("a".repeat(${overMaxLength})))`);
  await TestRunner.evaluateInPagePromise(`console.log(["a".repeat(${overMaxLength})])`);

  await ConsoleTestRunner.expandConsoleMessagesPromise();
  dumpMessageLengths();

  TestRunner.addResult('\nExpanding hidden texts');
  consoleView._visibleViewMessages.forEach(message => {
    message.element().querySelectorAll('.expandable-inline-button').forEach(button => button.click());
  });

  dumpMessageLengths();
  TestRunner.completeTest();

  function dumpMessageLengths() {
    consoleView._visibleViewMessages.forEach((message, index) => {
      const text = consoleMessageText(index);
      TestRunner.addResult(`Message: ${index}, length: ${text.length}, ${text}`);
    });

    function consoleMessageText(index) {
      const messageElement = consoleView._visibleViewMessages[index].element();
      const anchor = messageElement.querySelector('.console-message-anchor');
      if (anchor)
        anchor.remove();
      const links = messageElement.querySelectorAll('.devtools-link');
      for (const link of links)
        TestRunner.addResult(`Link: ${link.textContent}`);
      return messageElement.deepTextContent();
    }
  }
})();
