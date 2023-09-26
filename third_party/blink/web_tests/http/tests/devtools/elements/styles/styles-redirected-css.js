// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that styles in redirected css are editable.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="test_div">test</div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/styles-redirected-css.php');

  ElementsTestRunner.selectNodeAndWaitForStyles('test_div', step1);

  function step1() {
    var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('background-color');
    treeItem.property.setDisabled(true).then(step2);
  }

  function step2(newStyle) {
    TestRunner.assertTrue(!!newStyle, 'Could not disable style in redirected css.');
    TestRunner.completeTest();
  }
})();
