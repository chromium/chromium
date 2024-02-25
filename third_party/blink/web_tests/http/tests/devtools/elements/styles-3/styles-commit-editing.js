// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests that editing is canceled properly after incremental editing.\n`);
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
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('color');
      treeOutline = treeElement.treeOutline;

      treeElement.startEditingName();
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
      TestRunner.selectTextInTextNode(treeElement.valueElement.firstChild);
      ElementsTestRunner.waitForStyleCommitted(next);
      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    function testNewPropertyEditorIsCreated(next) {
      var blankTreeElement = treeOutline.rootElement().childAt(1);
      if (!UIModule.UIUtils.isBeingEdited(blankTreeElement.nameElement)) {
        TestRunner.addResult('No new property editor active!');
        TestRunner.completeTest();
        return;
      }

      // Test Styles pane editor looping.
      ElementsTestRunner.waitForStyleCommitted(next);
      blankTreeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    function testCycleThroughPropertyEditing(next) {
      if (!UIModule.UIUtils.isBeingEdited(treeOutline.firstChild().nameElement)) {
        TestRunner.addResult('Original property name editor not active!');
        TestRunner.completeTest();
        return;
      }

      ElementsTestRunner.selectNodeWithId('other', next);
    },

    function testNodeStyles(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    async function dumpStyles(next) {
      TestRunner.addResult('After append:');
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      next();
    }
  ]);
})();
