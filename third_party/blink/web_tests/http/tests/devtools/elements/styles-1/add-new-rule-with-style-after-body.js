// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that adding a new rule works when there is a STYLE element after BODY. TIMEOUT SHOULD NOT OCCUR! Bug 111299 https://bugs.webkit.org/show_bug.cgi?id=111299\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="font-size: 12px">Text</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function addStyle()
      {
          var style = document.createElement("style");
          document.documentElement.appendChild(style);
          style.sheet.insertRule("foo {display: none;}", 0);
      }
  `);

  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, stylesheetAdded);
  TestRunner.evaluateInPage('addStyle()');

  function stylesheetAdded() {
    TestRunner.cssModel.removeEventListener(SDK.CSSModel.Events.StyleSheetAdded, stylesheetAdded);
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  }

  var treeElement;
  var hasResourceChanged;

  function step1() {
    ElementsTestRunner.addNewRule('inspected', step2);
  }

  function step2() {
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var newProperty = section.addNewBlankProperty();
    newProperty.startEditingName();
    newProperty.nameElement.textContent = 'color';
    newProperty.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    newProperty.valueElement.textContent = 'maroon';
    ElementsTestRunner.waitForStyleCommitted(step3);
    newProperty.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
}

  async function step3() {
    TestRunner.addResult('After adding new rule:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
