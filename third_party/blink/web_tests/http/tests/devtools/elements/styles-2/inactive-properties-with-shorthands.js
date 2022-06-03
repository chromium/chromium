// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that properties overridden by a shorthand are displayed as inactive in the sidebar.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected1 {
          /* Longhand overridden by shorthand */
          margin-top: 100px;
          margin: 0;
      }
      #inspected2 {
          /* Shorthand overridden by itself */
          padding: 100px;
          padding: 0;
      }
      #inspected3 {
          /* TODO: Shorthand overridden by a super-shorthand */
          border-width: 100px;
          border: 0 solid;
      }
      </style>
      <div id="inspected1">Test 1</div>
      <div id="inspected2">Test 2</div>
      <div id="inspected3">Test 3</div>
    `);

  for (let i = 1; i <= 3; ++i) {
    await new Promise((resolve) => {
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('inspected' + i, resolve);
    });
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
  }
  TestRunner.completeTest();
})();
