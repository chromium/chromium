// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that media query stack is computed correctly when several stylesheets share the same URL.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @media not print {
      #main {
          background: blue;
      }
      }
      </style>
      <style>
      @media not print {
      #main {
          color: white;
      }
      }
      </style>
      <div id="main"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('main', step1);

  async function step1() {
    TestRunner.addResult('Main style:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
