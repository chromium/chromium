// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that non-standard mixed-cased properties are displayed in the Styles pane.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #container {
        -webkit-FONT-smoothing: antialiased;
      }
      </style>
      <div id="container" style="CoLoR: blAck">
          <div id="nested"></div>
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('nested', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles();
    TestRunner.completeTest();
  }
})();
