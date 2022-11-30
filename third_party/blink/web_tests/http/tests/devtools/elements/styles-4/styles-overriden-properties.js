// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that overriding shorthands within rule are visible.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #main {
          height: 100px;
          background: #000;
          background: #bada55;
      }
      #main {
          -x:
      }
      #main {
          -x:
      }
      </style>
      <div id="main"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('main', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
