// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that proper source lines are reported for the styles after unrecognizer / invalid selector.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      /* Invalid selector */
      .navbar-search .search-query:-moz-placeholder {
        color: #cccccc;
      }

      #container {
        padding: 15px;
      }
      </style>
      <div id="container"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('container', step1);

  async function step1() {
    TestRunner.addResult('Initial value');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

    var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('padding');
    treeItem.applyStyleText('padding: 20px', false);
    ElementsTestRunner.waitForStyles('container', step2);
  }

  async function step2() {
    TestRunner.addResult('After changing property');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
