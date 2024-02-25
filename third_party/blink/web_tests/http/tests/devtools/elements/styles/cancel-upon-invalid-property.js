// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that entering poor property value restores original text.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          color: blue;
      }
      </style>
      <div id="inspected">Text</div>
    `);

  var treeElement;
  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', editProperty);

  async function editProperty() {
    treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    treeElement.startEditingName();
    treeElement.nameElement.textContent = 'color';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    treeElement.valueElement.textContent = 'red';
    treeElement.kickFreeFlowStyleEditForTest().then(commitInvalidProperty);
  }

  function commitInvalidProperty() {
    treeElement.valueElement.textContent = 'red/*';
    ElementsTestRunner.waitForStyleCommitted(dumpAndExit);
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  async function dumpAndExit() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
