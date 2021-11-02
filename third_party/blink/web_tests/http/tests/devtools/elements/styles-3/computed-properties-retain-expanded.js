// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that Computed Style preserves property expansion on re-rendering.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      #inspected {
          display: flex;
          color: blue;
      }

      #other {
          display: inline;
      }

      div {
          display: block;
          color: black;
      }

      </style>
      <div id="inspected">Inspected</div>
      <div id="other">Other</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('inspected', step1);

  async function step1() {
    var treeElement = ElementsTestRunner.findComputedPropertyWithName('display');
    treeElement.expand();
    TestRunner.addResult('\n#inspected computed styles: ');
    await ElementsTestRunner.dumpComputedStyle(true);
    ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('other', step2);
  }

  async function step2() {
    TestRunner.addResult('\n#other computed styles: ');
    await ElementsTestRunner.dumpComputedStyle(true);
    TestRunner.completeTest();
  }
})();
