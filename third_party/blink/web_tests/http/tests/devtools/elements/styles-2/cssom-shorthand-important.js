// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that CSSOM-modified shorthands are reporting their "important" bits.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
        padding: 10px 50px !important;
      }
      </style>
      <div id="inspected">Text</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      document.styleSheets[0].cssRules[0].style.marginTop = "10px"
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', dump);

  async function dump() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
