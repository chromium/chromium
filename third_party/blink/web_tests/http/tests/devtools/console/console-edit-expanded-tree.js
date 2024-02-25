// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult('Tests that expanded tree element is editable in console.\n');

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
    var messages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;

    for (var i = 0; i < messages.length; ++i) {
      var message = messages[i];
      var node = message.contentElement();

      for (var node = message.contentElement(); node; node = node.traverseNextNode(message.contentElement())) {
        const treeElement = UIModule.TreeOutline.TreeElement.getTreeElementBylistItemNode(node);
        if (treeElement) {
          onTreeElement(treeElement.firstChild());
          return;
        }
      }
    }
  }

  function onTreeElement(treeElement) {
    treeElement.startEditing();
    Console.ConsoleView.ConsoleView.instance().viewport.refresh();
    TestRunner.addResult('After viewport refresh tree element remains in editing mode: ' + !!treeElement.prompt);
    TestRunner.completeTest();
  }
})();
