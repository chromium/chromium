// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that HTML editor hides only when focusing another element\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="inspected">Inspected Node</div>
      </div>
    `);

  const treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function selectNode(next) {
      ElementsTestRunner.selectNodeWithId('inspected', onNodeSelected);

      function onNodeSelected(node) {
        var treeElement = treeOutline.findTreeElement(node);
        treeElement.toggleEditAsHTML();
        TestRunner.addSniffer(Elements.ElementsTreeOutline.prototype, 'setMultilineEditing', next);
      }
    },

    function testBlurWithoutRelatedTarget(next) {
      const activeElement = document.deepActiveElement();
      TestRunner.addResult(`Active element: ${activeElement.tagName}`);
      activeElement.blur();
      dumpIsEditing();
      activeElement.focus();
      next();
    },

    function testBlurWithRelatedTarget(next) {
      const activeElement = document.deepActiveElement();
      TestRunner.addResult(`Active element: ${activeElement.tagName}`);
      const dummy = createElement('button');
      document.body.appendChild(dummy);
      dummy.focus();
      dumpIsEditing();
      next();
    },
  ]);

  function dumpIsEditing() {
    TestRunner.addResult(`Is editing: ${treeOutline.editing()}`);
  }
})();
