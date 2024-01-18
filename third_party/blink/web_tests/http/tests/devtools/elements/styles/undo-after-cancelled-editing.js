// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verifies that cancelling property value editing doesn't affect undo stack.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
      }
      </style>

      <div id="inspected">Text</div>
    `);

  var treeElement;
  TestRunner.runTestSuite([
    function selectNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function addNewProperty(next) {
      var section = ElementsTestRunner.firstMatchedStyleSection();
      var newProperty = section.addNewBlankProperty();
      newProperty.startEditingName();
      newProperty.nameElement.textContent = 'color';
      newProperty.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
      newProperty.valueElement.textContent = 'blue';
      ElementsTestRunner.waitForStyleCommitted(next);
      newProperty.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    async function editProperty(next) {
      treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
      treeElement.startEditingName();
      treeElement.nameElement.textContent = 'color';
      treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

      // Update incrementally, do not commit.
      treeElement.valueElement.textContent = 'red';
      treeElement.kickFreeFlowStyleEditForTest().then(next);
    },

    function cancelEditing(next) {
      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Escape'));
      ElementsTestRunner.waitForStyleApplied(next);
    },

    async function undoStyles(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
      SDK.DOMModel.DOMModelUndoStack.instance().undo();
      ElementsTestRunner.waitForStyles('inspected', next, true);
    },

    async function onUndoedProperty(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
      next();
    }
  ]);
})();
