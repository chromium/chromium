// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that renaming a selector updates element styles. Bug 70018. https://bugs.webkit.org/show_bug.cgi?id=70018\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
        color: green;
      }
      </style>

      <div id="inspected" style="color: red">Text</div>
      <div id="other"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    TestRunner.addResult('=== Before selector modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = ElementsTestRunner.firstMatchedStyleSection();
    section.startEditingSelector();
    section.selectorElement.textContent = 'hr, #inspected ';
    ElementsTestRunner.waitForSelectorCommitted(step2);
    section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  async function step2() {
    TestRunner.addResult('=== After non-affecting selector modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = ElementsTestRunner.firstMatchedStyleSection();
    section.startEditingSelector();
    section.selectorElement.textContent = '#inspectedChanged';
    ElementsTestRunner.waitForSelectorCommitted(step3);
    section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  async function step3() {
    TestRunner.addResult('=== After affecting selector modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
