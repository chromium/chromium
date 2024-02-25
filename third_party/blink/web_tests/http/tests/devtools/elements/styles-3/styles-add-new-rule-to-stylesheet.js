// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that adding new rule in the stylesheet end works as expected.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div class="inspected">Styled element</div>
      <div id="inspect"></div>
      <div id="other"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/different-rule-types.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('inspect', fetchStyleSheet);

  function fetchStyleSheet() {
    var styleSheets = TestRunner.cssModel.allStyleSheets();
    for (var i = 0; i < styleSheets.length; ++i) {
      var styleSheetHeader = styleSheets[i];
      if (styleSheetHeader.sourceURL.indexOf('different-rule-types.css') === -1)
        continue;
      ElementsTestRunner.addNewRuleInStyleSheet(styleSheetHeader, '#other, div', onRuleAdded);
      return;
    }
  }

  function onRuleAdded() {
    ElementsTestRunner.selectNodeAndWaitForStyles('other', onNodeSelected);
  }

  async function onNodeSelected() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
