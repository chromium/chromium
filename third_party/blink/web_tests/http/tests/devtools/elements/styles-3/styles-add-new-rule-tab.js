// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests that adding a new rule works after switching nodes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="font-size: 12px">Text</div>
      <div id="other" style="color:red"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  TestRunner.addSniffer(Workspace.UISourceCode.UISourceCode.prototype, 'addRevision', onRevisionAdded);

  var treeElement;
  var hasResourceChanged;

  var testFinished = false;
  var revisionAdded = false;
  var displayName = '';

  function step1() {
    // Click "Add new rule".
    ElementsTestRunner.addNewRule('foo, div#inspected, bar', step2);
  }

  function step2() {
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var newProperty = section.addNewBlankProperty();
    newProperty.startEditing();
    textInputController.insertText('color');
    newProperty.nameElement.dispatchEvent(TestRunner.createKeyEvent('Tab'));
    textInputController.insertText('maroon');
    newProperty.valueElement.dispatchEvent(TestRunner.createKeyEvent('Tab'));
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step3);
  }

  function step3() {
    // Click "Add new rule".
    ElementsTestRunner.addNewRule(null, onRuleAdded);

    function onRuleAdded() {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step4);
    }
  }

  async function step4() {
    TestRunner.addResult('After adding new rule (inspected):');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true, true);
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step5);
  }

  async function step5() {
    TestRunner.addResult('After adding new rule (other):');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    testFinished = true;
    maybeCompleteTest();
  }

  function maybeCompleteTest() {
    if (!testFinished || !revisionAdded)
      return;
    TestRunner.addResult('Revision added: ' + displayName);
    TestRunner.completeTest();
  }

  function onRevisionAdded(revision) {
    displayName = this.displayName();
    revisionAdded = true;
    maybeCompleteTest();
  }
})();
