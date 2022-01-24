// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that elements panel shows proper styles in the sidebar panel.\n`);

  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  await TestRunner.loadHTML(`
    <div id="container">
        <div id="foo" class="foo" style="display:none; -webkit-font-smoothing: subpixel-antialiased;" align="left">Foo</div>
    </div>
  `);

  await TestRunner.addStylesheetTag('resources/elements-panel-styles.css');

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('foo', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.completeTest();
  }
})();
