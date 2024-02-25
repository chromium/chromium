// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {AccessibilityTestRunner} from 'accessibility_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Accessibility from 'devtools/panels/accessibility/accessibility.js';

(async function() {
  TestRunner.addResult(`Tests that writing an ARIA attribute causes the accessibility node to be updated.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <button id="inspected" role="checkbox" aria-checked="true">ARIA checkbox</button>
    `);

  await UI.ViewManager.ViewManager.instance().showView('accessibility.view')
      .then(() => AccessibilityTestRunner.selectNodeAndWaitForAccessibility('inspected'))
      .then(editAriaChecked);

  function editAriaChecked() {
    TestRunner.addResult('=== Before attribute modification ===');
    AccessibilityTestRunner.dumpSelectedElementAccessibilityNode();
    var treeElement = AccessibilityTestRunner.findARIAAttributeTreeElement('aria-checked');
    treeElement.startEditing();
    treeElement.prompt.element().textContent = 'false';
    treeElement.prompt.element().dispatchEvent(TestRunner.createKeyEvent('Enter'));
    Accessibility.AccessibilitySidebarView.AccessibilitySidebarView.instance().doUpdate().then(() => {
      editRole();
    });
  }

  function editRole() {
    TestRunner.addResult('=== After attribute modification ===');
    AccessibilityTestRunner.dumpSelectedElementAccessibilityNode();
    var treeElement = AccessibilityTestRunner.findARIAAttributeTreeElement('role');
    treeElement.startEditing();
    treeElement.prompt.element().textContent = 'radio';
    treeElement.prompt.element().dispatchEvent(TestRunner.createKeyEvent('Enter'));
    // Give the document lifecycle a chance to run before updating the view.
    window.setTimeout(() => {
      Accessibility.AccessibilitySidebarView.AccessibilitySidebarView.instance().doUpdate().then(() => {
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
