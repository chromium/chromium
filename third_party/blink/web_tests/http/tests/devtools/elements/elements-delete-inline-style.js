// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that the "style" attribute removal results in the Styles sidebar pane update (not a crash). Bug 51478\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="color: red"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1(node) {
    TestRunner.addResult('Before style property removal:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    node.removeAttribute('style');
    ElementsTestRunner.waitForStyles('inspected', step2);
  }

  async function step2() {
    TestRunner.addResult('After style property removal:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
