// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Elements from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that links are updated properly when editing selector.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #pseudo::after {
          pseudo-property: "12";
          color: red;
      }

      #pseudo::after {
          border: 1px solid black;
      }

      #pseudo::before {
          color: blue;
      }
      </style>
      <div id="container" class="left-intact should-change">
      Red text here.
      </div>

      <div id="other"></div>

      <section id="pseudo">



      </section>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/styles-update-links-2.css');
  await TestRunner.addStylesheetTag('../styles/resources/styles-update-links.css');

  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('container', next);
    },

    function testEditSelector(next) {
      var section =
          Elements.ElementsPanel.ElementsPanel.instance().stylesWidget.sectionBlocks[0].sections[3];
      section.startEditingSelector();
      section.selectorElement.textContent = '.should-change, .INSERTED-OTHER-SELECTOR';
      ElementsTestRunner.waitForSelectorCommitted(onSelectorEdited);
      section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

      async function onSelectorEdited() {
        TestRunner.addResult('\n\n#### AFTER SELECTOR EDIT ####\n\n');
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
        var rules = ElementsTestRunner.getMatchedRules();
        ElementsTestRunner.validateRuleRanges('container', rules, next);
      }
    }
  ]);
})();
