// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that properties defining grid templates are correct.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
        #container { grid: "a a"; }
        #container { grid:   " a a "    " b b" ; }
        #container { grid: "a a" "b b"; }
        #container { grid: "a a" 'b b'; }
        #container { grid: "a a" 10px 'b b' 20px / 100px; }
        #container { grid: [first-row-start] "a a" 10px [first-row-end] [second-row-start] "b b" 20px / 100px; }

      /*# sourceURL=styles-grid-template.js*/
      </style>
      <div id="container" style="grid-template-areas: 'a a' 'b b'; grid-template: 'a a'"></div>
    `);

  TestRunner.evaluateInPage('loadIframe()');
  ElementsTestRunner.selectNodeAndWaitForStyles('container', step1);

  function step1() {
    ElementsTestRunner.dumpSelectedElementStyles(true /** excludeComputed */,
                                                 false,
                                                 true /** omitLonghands */,
                                                 false,
                                                 true /** printInnerText */);
    step2();
  }

  function step2() {
    const treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('grid-template-areas');
    TestRunner.addResult('Start editing "grid-template-areas"');
    treeElement.startEditing(treeElement.valueElement);

    TestRunner.addResult(`Prompt text ${treeElement._prompt.text()}`);

    TestRunner.completeTest();
  }
})();
