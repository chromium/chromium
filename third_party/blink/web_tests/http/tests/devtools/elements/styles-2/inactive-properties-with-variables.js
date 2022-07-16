// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that longhands overridden by a shorthand with var() are displayed as inactive in the sidebar.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          margin-top: 1px;
      }
      #inspected {
          margin: var(--m, 100px);
      }
      </style>
      <div id="inspected">Test</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('inspected', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.completeTest();
  }
})();
