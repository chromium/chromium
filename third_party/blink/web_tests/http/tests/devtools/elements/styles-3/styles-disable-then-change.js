// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that changing a disabled property enables it as well.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="font-weight:bold">
      </div>

      <div id="other">
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('container', step1);

  async function step1() {
    TestRunner.addResult('Before disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);

    ElementsTestRunner.toggleStyleProperty('font-weight', false);
    ElementsTestRunner.waitForStyles('container', step2);
  }

  async function step2() {
    TestRunner.addResult('After disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);

    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    treeItem.applyStyleText('color: green', false);
    ElementsTestRunner.waitForStyles('container', step3);
  }

  async function step3() {
    TestRunner.addResult('After change');
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);
    TestRunner.completeTest();
  }
})();
