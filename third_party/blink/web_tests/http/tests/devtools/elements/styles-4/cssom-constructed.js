// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that constructed stylesheets appear properly.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      const s = new CSSStyleSheet();
      s.replaceSync('div {color: red}');
      document.adoptedStyleSheets = [s];
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', dump);

  function dump() {
    ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
