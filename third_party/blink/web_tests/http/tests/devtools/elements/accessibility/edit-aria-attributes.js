// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that writing an ARIA attribute causes the accessibility node to be updated.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('accessibility_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <button id="inspected" role="checkbox" aria-checked="true">ARIA checkbox</button>
    `);

  UI.viewManager.showView('accessibility.view')
      .then(() => AccessibilityTestRunner.selectNodeAndWaitForAccessibility('inspected'))
      .then(editAriaChecked);

  function editAriaChecked() {
    TestRunner.addResult('=== Before attribute modification ===');
    AccessibilityTestRunner.dumpSelectedElementAccessibilityNode();
    var treeElement = AccessibilityTestRunner.findARIAAttributeTreeElement('aria-checked');
    treeElement._startEditing();
    treeElement._prompt._element.textContent = 'false';
    treeElement._prompt._element.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    self.runtime.sharedInstance(Accessibility.AccessibilitySidebarView).doUpdate().then(() => {
      editRole();
    });
  }

  function editRole() {
    TestRunner.addResult('=== After attribute modification ===');
    AccessibilityTestRunner.dumpSelectedElementAccessibilityNode();
    var treeElement = AccessibilityTestRunner.findARIAAttributeTreeElement('role');
    treeElement._startEditing();
    treeElement._prompt._element.textContent = 'radio';
    treeElement._prompt._element.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    // Give the document lifecycle a chance to run before updating the view.
    window.setTimeout(() => {
      self.runtime.sharedInstance(Accessibility.AccessibilitySidebarView)
          .doUpdate()
          .then(() => {
            postRoleChange();
          });
    }, 0);
  }

  function postRoleChange() {
    TestRunner.addResult('=== After role modification ===');
    AccessibilityTestRunner.dumpSelectedElementAccessibilityNode();
    TestRunner.completeTest();
  }
})();
