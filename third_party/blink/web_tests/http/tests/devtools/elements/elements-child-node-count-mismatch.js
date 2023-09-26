// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  'use strict';
  TestRunner.addResult(`Tests that Elements properly populate and select after immediate updates crbug.com/829884\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML">
      <html>
      <head></head>
      <body id="body">
        <div>FooBar1</div>
        <div>FooBar2</div>
      </body>
      </html>
    `);

  const treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  ElementsTestRunner.selectNodeWithId('body', node => {
    TestRunner.addResult(`BEFORE: children: ${node.children()}, childNodeCount: ${node.childNodeCount()}`);

    // Any operation that modifies the node, followed by an immediate, synchronous update.
    TestRunner.domModel.childNodeCountUpdated(node.id, 3);
    treeOutline.updateModifiedNodes();

    TestRunner.addResult(`AFTER: children: ${node.children()}, childNodeCount: ${node.childNodeCount()}`);
    ElementsTestRunner.expandElementsTree(afterExpand);
  });

  function afterExpand() {
    ElementsTestRunner.selectNodeWithId('body', node => {
      const treeElement = treeOutline.treeElementByNode.get(node);
      TestRunner.addResult(`AFTER EXPAND: TreeElement childCount: ${treeElement.childCount()}`);

      var selectedElement = treeOutline.selectedTreeElement;
      var nodeName = selectedElement ? selectedElement.node().nodeName() : 'null';
      TestRunner.addResult('Selected element:\'' + nodeName + '\'');
      TestRunner.completeTest();
    });
  }
})();
