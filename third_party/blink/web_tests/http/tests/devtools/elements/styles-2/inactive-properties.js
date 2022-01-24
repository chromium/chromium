// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that effectively inactive properties are displayed correctly in the sidebar.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          text-align: left;
          text-align: bar;
          text-align: right;
      }
      </style>
      <div id="container">
          <div id="inspected" align="left">Test</div>
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('inspected', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.completeTest();
  }
})();
