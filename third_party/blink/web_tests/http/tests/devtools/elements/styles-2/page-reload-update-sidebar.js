// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that reloading page during styles sidebar pane editing cancels editing and re-renders the sidebar pane.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="color: blue">Text</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function removeInspectedNode()
      {
          document.querySelector("#inspected").remove();
      }
  `);

  var stylesSidebarPane = UI.panels.elements.stylesWidget;
  TestRunner.runTestSuite([
    function selectInspectedNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function startEditingAndReloadPage(next) {
      var treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('color');
      var currentDocumentId = stylesSidebarPane.node().ownerDocument.id;
      treeElement.startEditing(treeElement.valueElement);
      var nodeRebuiltHappened = false;
      var pageReloadHappened = false;
      TestRunner.addSniffer(Elements.StylesSidebarPane.prototype, 'nodeStylesUpdatedForTest', onNodeRebuilt);
      TestRunner.reloadPage(reloadedCallback);

      function onNodeRebuilt(node, rebuild) {
        if (!node || node.ownerDocument.id === currentDocumentId) {
          TestRunner.addResult('ERROR: A rebuild update happened for the same node.');
          TestRunner.completeTest();
          return;
        }
        nodeRebuiltHappened = true;
        maybeNext();
      }

      function reloadedCallback() {
        pageReloadHappened = true;
        maybeNext();
      }

      function maybeNext() {
        if (nodeRebuiltHappened && pageReloadHappened)
          next();
      }
    },

    function onPageReloaded(next) {
      if (stylesSidebarPane.isEditingStyle) {
        TestRunner.addResult('StylesSidebarPane should not be locked in editing on page reload.');
        TestRunner.completeTest();
        return;
      }
      next();
    },
  ]);
})();
