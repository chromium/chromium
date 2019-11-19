// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that links are updated properly after disabling property.\n`);
  await TestRunner.loadModule('elements_test_runner');
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

    function testDisableProperty(next) {
      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('border');
      ElementsTestRunner.waitForStyleApplied(onPropertyDisabled);
      treeItem._toggleDisabled(true);

      function onPropertyDisabled() {
        TestRunner.addResult('\n\n#### AFTER PROPERTY DISABLED ####\n\n');
        ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
        var rules = ElementsTestRunner.getMatchedRules();
        ElementsTestRunner.validateRuleRanges('container', rules, next);
      }
    }
  ]);
})();
