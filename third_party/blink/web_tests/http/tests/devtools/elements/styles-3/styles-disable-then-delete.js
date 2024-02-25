// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that removal of property following its disabling works.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="font-weight:bold">
      </div>

      <div id="other">
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('container', step1);

  async function step1() {
    // Disable property
    TestRunner.addResult('Before disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);
    ElementsTestRunner.toggleStyleProperty('font-weight', false);
    ElementsTestRunner.waitForStyleApplied(step2);
  }

  async function step2() {
    // Delete style
    TestRunner.addResult('After disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);

    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    treeItem.applyStyleText('', false);

    ElementsTestRunner.waitForStyleApplied(step3);
  }

  function step3() {
    ElementsTestRunner.selectNodeWithId('other', step4);
  }

  function step4() {
    ElementsTestRunner.selectNodeAndWaitForStyles('container', step5);
  }

  async function step5(node) {
    TestRunner.addResult('After delete');
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);
    TestRunner.completeTest();
  }
})();
