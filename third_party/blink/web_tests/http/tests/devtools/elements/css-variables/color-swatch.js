// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that swatches for var() functions are updated as CSS variable is changed.`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      div {
        color: var(--bar);
        --bar: var(--baz);
        --baz: red;
      }

      #inspected {
        background-color: var(--bar);
      }
    </style>
    <div id="inspected"></div>
  `);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');
  TestRunner.addResult('Before css Variable editing:' );
  dumpSwatches();

  const bazTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('--baz');
  bazTreeElement.startEditing(bazTreeElement.valueElement);
  bazTreeElement.valueElement.textContent = 'blue';
  await bazTreeElement.kickFreeFlowStyleEditForTest();

  TestRunner.addResult('After css Variable editing:' );
  dumpSwatches();

  TestRunner.completeTest();

  function dumpSwatches() {
    const colorTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
    let swatch = colorTreeElement.valueElement.querySelector('span[is=color-swatch]');
    TestRunner.addResult('  "color" swatch:' + swatch.color().asString());

    const bgTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('background-color');
    swatch = bgTreeElement.valueElement.querySelector('span[is=color-swatch]');
    TestRunner.addResult('  "background-color" swatch:' + swatch.color().asString());
  }
})();
