// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `The patch verifies that color swatch functions properly in matched and computed styles. crbug.com/461363\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          color: red;
          --variable: red;
          background: var(--variable);
      }
      </style>
      <div id="inspected">Inspected div</div>
    `);

  TestRunner.runTestSuite([
    function selectNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('inspected', next);
    },

    function testColorSwatchInMatchRules(next) {
      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
      TestRunner.addResult('Initial color value: ' + treeItem.valueElement.textContent);
      var swatch =
          treeItem.valueElement.querySelector('span[is=color-swatch]').shadowRoot.querySelector('.color-swatch-inner');
      swatch.dispatchEvent(createShiftClick());
      TestRunner.addResult('After shift-click: ' + treeItem.valueElement.textContent);
      TestRunner.addResult('Has popover before click: ' + popoverVisible());

      swatch.click();
      TestRunner.addResult('Has popover after click: ' + popoverVisible());
      next();
    },

    function testColorSwatchInCustomProperty(next) {
      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('--variable');
      var swatch = treeItem.valueElement.querySelector('span[is=color-swatch]');
      TestRunner.addResult('Custom property has a color swatch: ' + !!swatch);
      next();
    },

    function testColorSwatchInVarFunction(next) {
      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('background');
      var swatch =
          treeItem.valueElement.querySelector('devtools-css-var-swatch')
              .shadowRoot.querySelector('.color-swatch-inner');
      TestRunner.addResult('var function has a color swatch: ' + !!swatch);
      next();
    },

    function testColorSwatchInComputedRules(next) {
      var computedProperty = ElementsTestRunner.findComputedPropertyWithName('color').title;
      var computedPropertyValue = computedProperty.querySelector('.value');
      TestRunner.addResult('Initial color value: ' + computedPropertyValue.textContent);
      var swatch =
          computedPropertyValue.querySelector('span[is=color-swatch]').shadowRoot.querySelector('.color-swatch-inner');
      swatch.dispatchEvent(createShiftClick());
      TestRunner.addResult('After shift-click color value: ' + computedPropertyValue.textContent);
      next();
    }
  ]);

  function createShiftClick() {
    const event = new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      detail: 1,
      screenX: 0,
      screenY: 0,
      clientX: 0,
      clientY: 0,
      shiftKey: true,
      composed: true
    });
    return event;
  }

  function popoverVisible() {
    return !!UI.GlassPane._panes.size;
  }
})();
