// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Elements from 'devtools/panels/elements/elements.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that keyframes are shown in styles pane.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @keyframes animName {
          from, 20% {
              margin-left: 200px;
              color: red;
          }
          to {
              margin-left: 500px;
          }
      }
      </style>
      <div id="element"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/keyframes.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('element', step1);

  async function step1() {
    TestRunner.addResult('=== Before key modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = Elements.ElementsPanel.ElementsPanel.instance().stylesWidget.sectionBlocks[1].sections[1];
    section.startEditingSelector();
    section.selectorElement.textContent = '1%';
    section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    ElementsTestRunner.waitForSelectorCommitted(step2);
  }

  async function step2() {
    TestRunner.addResult('=== After key modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    SDK.DOMModel.DOMModelUndoStack.instance().undo();
    ElementsTestRunner.waitForStyles('element', step3, true);
  }

  async function step3() {
    TestRunner.addResult('=== After undo ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    SDK.DOMModel.DOMModelUndoStack.instance().redo();
    ElementsTestRunner.waitForStyles('element', step4, true);
  }

  async function step4() {
    TestRunner.addResult('=== After redo ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = Elements.ElementsPanel.ElementsPanel.instance().stylesWidget.sectionBlocks[1].sections[1];
    section.startEditingSelector();
    section.selectorElement.textContent = '1% /*';
    section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    ElementsTestRunner.waitForSelectorCommitted(step5);
  }

  async function step5() {
    TestRunner.addResult('=== After invalid key modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
