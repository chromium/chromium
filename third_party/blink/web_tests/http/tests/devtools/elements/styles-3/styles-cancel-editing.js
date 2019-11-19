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

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  var treeElement;
  var section;

  async function step1() {
    ElementsTestRunner.dumpSelectedElementStyles(true);
    treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('color');

    treeElement.startEditing();
    treeElement.nameElement.textContent = 'color';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

    // Update incrementally, do not commit.
    treeElement.valueElement.textContent = 'green';
    await treeElement.kickFreeFlowStyleEditForTest();

    // Cancel editing.
    treeElement.valueElement.firstChild.select();
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Escape'));
    await ElementsTestRunner.waitForStyleAppliedPromise();

    ElementsTestRunner.selectNodeWithId('other', step2);
  }

  function step2() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step3);
  }

  function step3() {
    TestRunner.addResult('After append:');
    ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
