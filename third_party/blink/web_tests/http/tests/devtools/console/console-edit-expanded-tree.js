// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that expanded tree element is editable in console.\n');

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    (function onload()
    {
        var a = {};
        for (var i = 0; i < 100; ++i)
            a[i] = i;
        console.dir(a);

    })();
  `);

  ConsoleTestRunner.expandConsoleMessages(onConsoleMessageExpanded);

  function onConsoleMessageExpanded() {
    var messages = Console.ConsoleView.instance()._visibleViewMessages;

    for (var i = 0; i < messages.length; ++i) {
      var message = messages[i];
      var node = message.contentElement();

      for (var node = message.contentElement(); node; node = node.traverseNextNode(message.contentElement())) {
        const treeElement = UI.TreeElement.getTreeElementBylistItemNode(node);
        if (treeElement) {
          onTreeElement(treeElement.firstChild());
          return;
        }
      }
    }
  }

  function onTreeElement(treeElement) {
    treeElement._startEditing();
    Console.ConsoleView.instance()._viewport.refresh();
    TestRunner.addResult('After viewport refresh tree element remains in editing mode: ' + !!treeElement._prompt);
    TestRunner.completeTest();
  }
})();
