// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that computed styles expand and allow tracing to style rules.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      #inspected {
          background-color: green;
          font-family: Times;
      }

      #inspected {
          background-color: black;
          font-family: Courier;
      }

      #inspected {
          background: gray;
      }

      </style>
      <div id="inspected">
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    TestRunner.addResult('==== All styles (should be no computed) ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    ElementsTestRunner.computedStyleWidget().doUpdate().then(step2);
  }

  async function step2() {
    TestRunner.addResult('==== All styles (computed should be there) ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.completeTest();
  }
})();
