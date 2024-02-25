// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that sequence of setting selector and disabling property works.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          color: red;
      }
      </style>
      <div id="inspected">Red text here.</div>
    `);

  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function editSelector(next) {
      var section = ElementsTestRunner.firstMatchedStyleSection();
      section.startEditingSelector();
      section.selectorElement.textContent = '#inspected, .INSERTED-OTHER-SELECTOR';
      ElementsTestRunner.waitForSelectorCommitted(next);
      section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    function testDisableProperty(next) {
      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
      ElementsTestRunner.waitForStyleApplied(onPropertyDisabled);
      treeItem.toggleDisabled(true);

      async function onPropertyDisabled() {
        TestRunner.addResult('\n\n#### AFTER PROPERTY DISABLED ####\n\n');
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
        next();
      }
    }
  ]);
})();
