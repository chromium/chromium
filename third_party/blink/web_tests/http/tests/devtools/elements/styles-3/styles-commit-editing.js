// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that editing is canceled properly after incremental editing.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="color: red">Text</div>
      <div id="other"></div>
    `);

  var treeElement;
  var treeOutline;
  var section;

  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    async function testFreeFlowEdit(next) {
      ElementsTestRunner.dumpSelectedElementStyles(true);
      treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('color');
      treeOutline = treeElement.treeOutline;

      treeElement.startEditing();
      treeElement.nameElement.textContent = 'color';
      treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

      // Update incrementally, do not commit.
      treeElement.valueElement.textContent = 'rgb(/*';
      await treeElement.kickFreeFlowStyleEditForTest();
      next();
    },

    function testCommitEditing(next) {
      // Commit editing.
      treeElement.valueElement.textContent = 'green';
      treeElement.valueElement.firstChild.select();
      ElementsTestRunner.waitForStyleCommitted(next);
      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    function testNewPropertyEditorIsCreated(next) {
      var blankTreeElement = treeOutline.rootElement().childAt(1);
      if (!UI.isBeingEdited(blankTreeElement.nameElement)) {
        TestRunner.addResult('No new property editor active!');
        TestRunner.completeTest();
        return;
      }

      // Test Styles pane editor looping.
      ElementsTestRunner.waitForStyleCommitted(next);
      blankTreeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    function testCycleThroughPropertyEditing(next) {
      if (!UI.isBeingEdited(treeOutline.firstChild().nameElement)) {
        TestRunner.addResult('Original property name editor not active!');
        TestRunner.completeTest();
        return;
      }

      ElementsTestRunner.selectNodeWithId('other', next);
    },

    function testNodeStyles(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function dumpStyles(next) {
      TestRunner.addResult('After append:');
      ElementsTestRunner.dumpSelectedElementStyles(true);
      next();
    }
  ]);
})();
