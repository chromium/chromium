// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that adding an invalid property retains its syntax.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="font-size: 12px">Text</div>
      <div id="other"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  var treeElement;
  var section;

  async function step1() {
    TestRunner.addResult('Before append:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    section = ElementsTestRunner.inlineStyleSection();

    // Create and increment.
    treeElement = section.addNewBlankProperty();
    treeElement.startEditing();
    treeElement.nameElement.textContent = 'color';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

    // Update incrementally to a valid value.
    treeElement.valueElement.textContent = 'rgb(';
    treeElement.kickFreeFlowStyleEditForTest();
    ElementsTestRunner.waitForStyleApplied(step2);
  }

  function step2() {
    // Commit invalid value.
    treeElement.valueElement.textContent = 'rgb(1';
    ElementsTestRunner.waitForStyleApplied(step3);
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  function step3() {
    ElementsTestRunner.selectNodeWithId('other', step4);
  }

  function step4() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step5);
  }

  async function step5() {
    TestRunner.addResult('After append:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
