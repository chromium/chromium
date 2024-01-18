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

  var revisionAdded = false;
  var testFinished = false;
  var displayName = '';

  function step1() {
    ElementsTestRunner.addNewRule('foo, div#inspected, bar', step2);
  }

  function step2() {
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var newProperty = section.addNewBlankProperty();
    newProperty.startEditingName();
    textInputController.insertText('color');
    eventSender.keyDown(':');
    textInputController.insertText('maroon');
    ElementsTestRunner.waitForStyleApplied(step3);
    eventSender.keyDown(';');
  }

  function step3() {
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step4);
  }

  function step4() {
    ElementsTestRunner.addNewRule(null, onRuleAdded);

    function onRuleAdded() {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step5);
    }
  }

  async function step5() {
    TestRunner.addResult('After adding new rule (inspected):');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true, true);
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step6);
  }

  async function step6() {
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
    revisionAdded = true;
    displayName = this.displayName();
    maybeCompleteTest();
  }
})();
