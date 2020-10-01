// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests elements drag and drop operation internals, verifies post-move selection.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container">
          <div id="child1"></div>
          <div id="child2"></div>
          <div id="child3"></div>
          <div id="child4"></div>
      </div>
    `);

  var containerNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        containerNode = ElementsTestRunner.expandedNodeWithId('container');

        TestRunner.addResult('========= Original ========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      ElementsTestRunner.expandElementsTree(callback);
    },

    function testDragAndDrop(next) {
      var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
      treeOutline.addEventListener(Elements.ElementsTreeOutline.Events.SelectedNodeChanged, selectionChanged);

      function selectionChanged() {
        TestRunner.addResult('===== Moved child2 =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        TestRunner.addResult(
            'Selection: ' + Elements.DOMPath.fullQualifiedSelector(treeOutline.selectedDOMNode()));
        next();
      }

      var child2 = ElementsTestRunner.expandedNodeWithId('child2');
      var child4 = ElementsTestRunner.expandedNodeWithId('child4');
      treeOutline._treeElementBeingDragged = treeOutline.treeElementByNode.get(child2);
      var treeElementToDropOn = treeOutline.treeElementByNode.get(child4);
      treeOutline._doMove(treeElementToDropOn);
    }
  ]);
})();
