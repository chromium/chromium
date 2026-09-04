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

  await ConsoleTestRunner.expandConsoleMessagesPromise();
  if (ConsoleTestRunner.waitForAllPopulations) {
    await ConsoleTestRunner.waitForAllPopulations();
  }

  const messages =
      Console.ConsoleView.ConsoleView.instance().visibleViewMessages;
  for (let i = 0; i < messages.length; ++i) {
    const message = messages[i];

    for (let node = message.contentElement(); node;
         node = node.traverseNextNode(message.contentElement())) {
      const treeElement =
          UIModule.TreeOutline.TreeElement.getTreeElementBylistItemNode(node);
      if (treeElement) {
        if (!treeElement.firstChild() && treeElement.onpopulate) {
          await treeElement.onpopulate();
          await UIModule.Widget.Widget.allUpdatesComplete;
        }
        if (treeElement.firstChild()) {
          onTreeElement(treeElement.firstChild());
          return;
        }
      }
    }
  }

  function onTreeElement(treeElement) {
    treeElement.startEditing();
    Console.ConsoleView.ConsoleView.instance().viewport.refresh();
    TestRunner.addResult('After viewport refresh tree element remains in editing mode: ' + !!treeElement.editing);
    TestRunner.completeTest();
  }
})();
