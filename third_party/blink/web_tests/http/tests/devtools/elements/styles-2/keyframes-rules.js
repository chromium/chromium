// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that source data is extracted correctly from stylesheets with @keyframes rules.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @keyframes fadeout {
          from { background-color: black; }
          to { background-color: white; }
      }

      * {
          background-color: papayawhip;
      }
      </style>
      <div id="inspected" style="background-color: white;">Content</div>
    `);

  TestRunner.runTestSuite([
    function testInit(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    async function testDumpStyles(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      next();
    }
  ]);
})();
