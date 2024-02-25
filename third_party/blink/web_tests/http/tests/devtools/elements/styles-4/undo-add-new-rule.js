// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that adding a new rule can be undone.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div class="foo" id="inspected" style="font-size: 12px">Text</div>
      <div class="foo" id="other" style="color:red"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  var treeElement;

  function step1() {
    addNewRuleAndSelectNode('other', step2);
  }

  function step2() {
    addNewRuleAndSelectNode('inspected', step3);
  }

  async function step3() {
    TestRunner.addResult('After adding new rule:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    printStyleSheetAndCall(step4);
  }

  function step4() {
    SDK.DOMModel.DOMModelUndoStack.instance().undo();
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step5);
  }

  async function step5() {
    TestRunner.addResult('After undo:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    printStyleSheetAndCall(step6);
  }

  function step6() {
    SDK.DOMModel.DOMModelUndoStack.instance().redo();
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step7);
  }

  async function step7() {
    TestRunner.addResult('After redo:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    printStyleSheetAndCall(step8);
  }

  function step8() {
    TestRunner.completeTest();
  }

  function addNewRuleAndSelectNode(nodeId, next) {
    function selectNode() {
      ElementsTestRunner.selectNodeAndWaitForStyles(nodeId, next);
    }

    ElementsTestRunner.addNewRule('div.foo', selectNode);
  }

  async function printStyleSheetAndCall(next) {
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var id = section.style().styleSheetId;
    var styleSheetText = await TestRunner.CSSAgent.getStyleSheetText(id);
    TestRunner.addResult('===== Style sheet text: =====');
    TestRunner.addResult(styleSheetText);
    TestRunner.addResult('=============================');
    next();
  }
})();
