// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verifies inspector doesn't break when switching panels while editing as HTML. crbug.com/485457\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="inspected">Inspected Node</div>
      </div>
    `);

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function selectNode(next) {
      ElementsTestRunner.selectNodeWithId('inspected', onNodeSelected);

      function onNodeSelected(node) {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        var treeElement = treeOutline.findTreeElement(node);
        treeElement.toggleEditAsHTML();
        TestRunner.addSniffer(Elements.ElementsTreeOutline.prototype, 'setMultilineEditing', next);
      }
    },

    function switchPanels(next) {
      UI.inspectorView.showPanel('sources').then(next).catch(onError);

      function onError(error) {
        TestRunner.addResult('FAILURE: exception caught while switching panels.');
        TestRunner.completeTest();
      }
    },

    async function switchBackToElements(next) {
      await UI.inspectorView.showPanel('elements');
      const treeOutline = ElementsTestRunner.firstElementsTreeOutline();
      TestRunner.addResult(`Is editing: ${treeOutline.editing()}`);
      next();
    }
  ]);
})();
