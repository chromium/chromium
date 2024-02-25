// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that filtering in StylesSidebarPane works as expected.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .mydiv {
          border: 1px solid black;
          padding: 10px 10px 10px 10px;
      }

      #inspected {
          border-size: 2px;
      }

      </style>
      <div style="margin: 1px;" class="mydiv" id="inspected"></div>
    `);

  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function testSimpleFiltering(next) {
      ElementsTestRunner.filterMatchedStyles('padding');
      ElementsTestRunner.dumpRenderedMatchedStyles();
      next();
    },

    function testLonghandsAreAutoExpanded(next) {
      ElementsTestRunner.filterMatchedStyles('-top');
      ElementsTestRunner.dumpRenderedMatchedStyles();
      next();
    },

    function testAutoExpandedLonghandsAreCollapsed(next) {
      ElementsTestRunner.filterMatchedStyles(null);
      ElementsTestRunner.dumpRenderedMatchedStyles();
      next();
    }
  ]);
})();
