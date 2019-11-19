// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that adding a new blank property works.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="font-size: 12px">Text</div>
      <div id="other"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', addAndIncrementFirstProperty);

  var treeElement;
  var section;

  function addAndIncrementFirstProperty() {
    TestRunner.addResult('Before append:');
    ElementsTestRunner.dumpSelectedElementStyles(true);
    section = ElementsTestRunner.inlineStyleSection();

    // Create and increment.
    treeElement = section.addNewBlankProperty(0);
    treeElement.startEditing();
    treeElement.nameElement.textContent = 'margin-left';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

    treeElement.valueElement.textContent = '1px';
    treeElement.valueElement.firstChild.select();
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('ArrowUp'));
    ElementsTestRunner.waitForStyleApplied(incrementProperty);
  }

  function incrementProperty() {
    // Increment again.
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('ArrowUp'));
    ElementsTestRunner.waitForStyleApplied(commitProperty);
  }

  async function commitProperty() {
    // Commit.
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    await ElementsTestRunner.waitForStyleAppliedPromise();
    reloadStyles(addAndChangeLastCompoundProperty);
  }

  async function addAndChangeLastCompoundProperty() {
    TestRunner.addResult('After insertion at index 0:');
    ElementsTestRunner.dumpSelectedElementStyles(true);

    treeElement = ElementsTestRunner.inlineStyleSection().addNewBlankProperty(2);
    treeElement.startEditing();
    treeElement.nameElement.textContent = 'color';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

    treeElement.valueElement.textContent = 'green; font-weight: bold';
    await treeElement.kickFreeFlowStyleEditForTest();

    treeElement.valueElement.textContent = 'red; font-weight: bold';
    await treeElement.kickFreeFlowStyleEditForTest();

    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    await ElementsTestRunner.waitForStyleAppliedPromise();
    reloadStyles(addAndCommitMiddleProperty);
  }

  async function addAndCommitMiddleProperty() {
    TestRunner.addResult('After appending and changing a \'compound\' property:');
    ElementsTestRunner.dumpSelectedElementStyles(true);

    treeElement = ElementsTestRunner.inlineStyleSection().addNewBlankProperty(2);
    treeElement.startEditing();
    treeElement.nameElement.textContent = 'third-property';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    treeElement.valueElement.textContent = 'third-value';

    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    await ElementsTestRunner.waitForStyleAppliedPromise();
    reloadStyles(dumpAndComplete);
  }

  function dumpAndComplete() {
    TestRunner.addResult('After insertion at index 2:');
    ElementsTestRunner.dumpSelectedElementStyles(true);

    TestRunner.completeTest();
  }

  function reloadStyles(callback) {
    ElementsTestRunner.selectNodeAndWaitForStyles('other', otherCallback);

    function otherCallback() {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', callback);
    }
  }
})();
