// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that editing a CSS property name in the Styles pane retains its original, non-trimmed value text.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAA5JREFUeNpiYGBgAAgwAAAEAAGbA+oJAAAAAElFTkSuQmCC)">



      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  function step1() {
    var treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('background');
    TestRunner.addResult('Viewing \'background\' value in Styles:');
    TestRunner.addResult(treeElement.valueElement.textContent);

    treeElement.startEditingName();
    treeElement.nameElement.textContent = 'background-image';
    ElementsTestRunner.waitForStyleCommitted(step2);
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  function step2() {
    var treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('background-image');
    TestRunner.addResult('Renamed \'background\' to \'background-image\' (edited value):');
    TestRunner.addResult(treeElement.valueElement.textContent);
    TestRunner.completeTest();
  }
})();
