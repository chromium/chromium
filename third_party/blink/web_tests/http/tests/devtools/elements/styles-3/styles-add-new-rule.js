// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests that adding a new rule works after switching nodes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="font-size: 12px">Text</div>
      <div id="other" style="color:red"></div>
      <div>
          <div class="my-class"></div>
          <div class="my-class"></div>
          <div class="my-class"></div>
      </div>

      <div class=" class-1 class-2  class-3   "></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  var treeElement;
  var hasResourceChanged;

  var revisionAdded = false;
  var testFinished = false;
  var displayName = null;

  TestRunner.addSniffer(Workspace.UISourceCode.UISourceCode.prototype, 'addRevision', onRevisionAdded);

  function step1() {
    // Click "Add new rule".
    ElementsTestRunner.addNewRule('foo, div#inspected, bar', step2);
  }

  function step2() {
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var newProperty = section.addNewBlankProperty();
    newProperty.startEditing();
    textInputController.insertText('color');
    newProperty.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    textInputController.insertText('maroon');
    newProperty.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
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

    ElementsTestRunner.waitForStylesForClass('my-class', onStylesReceived);
    ElementsTestRunner.nodeWithClass('my-class', onNodeFound);
    function onNodeFound(node) {
      Common.Revealer.reveal(node);
    }

    function onStylesReceived() {
      ElementsTestRunner.addNewRule(null, step6);
    }
  }

  async function step6() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

    ElementsTestRunner.waitForStylesForClass('class-1', onStylesReceived);
    ElementsTestRunner.nodeWithClass('class-1', onNodeFound);
    function onNodeFound(node) {
      Common.Revealer.reveal(node);
    }

    function onStylesReceived() {
      ElementsTestRunner.addNewRule(null, async function() {
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
        testFinished = true;
        maybeCompleteTest();
      });
    }
  }

  function maybeCompleteTest() {
    if (!revisionAdded || !testFinished)
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
