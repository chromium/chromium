// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that changing selected node while editing style does update styles sidebar.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="color: red">Text</div>
      <div id="other" style="color: blue"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  var treeElement;
  var section;

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, true, true);
    treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('color');

    treeElement.startEditing();
    treeElement.nameElement.textContent = 'background';

    ElementsTestRunner.selectNodeAndWaitForStyles('other', step2);
  }

  async function step2() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, true, true);
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step3);
  }

  async function step3() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, true, true);
    TestRunner.completeTest();
  }
})();
