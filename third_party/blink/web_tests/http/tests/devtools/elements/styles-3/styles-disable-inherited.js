// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that disabling inherited style property does not break further style inspection.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="font-weight:bold">
          <div id="nested"></div>
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('nested', step1);

  async function step1() {
    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    TestRunner.addResult('Before disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    ElementsTestRunner.selectNodeAndWaitForStyles('container', step2);
  }

  function step2() {
    ElementsTestRunner.toggleStyleProperty('font-weight', false);
    ElementsTestRunner.selectNodeAndWaitForStyles('nested', step3);
  }

  async function step3() {
    TestRunner.addResult('After disable:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
