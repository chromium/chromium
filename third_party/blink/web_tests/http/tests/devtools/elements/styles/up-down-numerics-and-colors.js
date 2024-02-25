// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Host from 'devtools/core/host/host.js';

(async function() {
  TestRunner.addResult(`Tests that numeric and color values are incremented/decremented correctly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      h1 {
          color: #FF2;
          opacity: .5;
          transform: rotate(1000000000000000065537deg);
      }
      </style>
      <h1 id="inspected">Inspect Me</h1>
    `);

  TestRunner.runTestSuite([
    function testInit(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function testAlterColor(next) {
      var colorTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
      colorTreeElement.startEditingValue();

      // PageUp should change to 'FF3'
      colorTreeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('PageUp'));
      // Ctrl/Meta + Shift Down should change to 'EE3'
      if (Host.Platform.isMac())
        colorTreeElement.valueElement.dispatchEvent(
            TestRunner.createKeyEvent('ArrowDown', /*Ctrl*/ false, /*Alt*/ false, /*Shift*/ true, /*Meta*/ true));
      else
        colorTreeElement.valueElement.dispatchEvent(
            TestRunner.createKeyEvent('ArrowDown', /*Ctrl*/ true, /*Alt*/ false, /*Shift*/ true, /*Meta*/ false));

      TestRunner.addResult(TestRunner.textContentWithoutStyles(colorTreeElement.listItemElement));
      next();
    },

    function testAlterNumber(next) {
      var opacityTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('opacity');
      opacityTreeElement.startEditingValue();
      // 0.5 (initial). Alt + Up should change to 0.6
      opacityTreeElement.valueElement.dispatchEvent(
          TestRunner.createKeyEvent('ArrowUp', /*Ctrl*/ false, /*Alt*/ true, /*Shift*/ false));
      // Up should change to 1.6
      opacityTreeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('ArrowUp'));
      // Shift + PageUp should change to 11.6
      opacityTreeElement.valueElement.dispatchEvent(
          TestRunner.createKeyEvent('PageUp', /*Ctrl*/ false, /*Alt*/ false, /*Shift*/ true));
      TestRunner.addResult(TestRunner.textContentWithoutStyles(opacityTreeElement.listItemElement));
      next();
    },

    function testAlterBigNumber(next) {
      var treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('transform');
      treeElement.startEditingValue();
      var selection = treeElement.valueElement.getComponentSelection();
      var range = selection.getRangeAt(0);
      var newRange = document.createRange();
      newRange.setStart(range.startContainer, 10);
      newRange.setEnd(range.startContainer, 10);
      selection.removeAllRanges();
      selection.addRange(newRange);
      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('ArrowUp'));
      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('PageUp'));
      TestRunner.addResult(TestRunner.textContentWithoutStyles(treeElement.listItemElement));
      next();
    }
  ]);
})();
