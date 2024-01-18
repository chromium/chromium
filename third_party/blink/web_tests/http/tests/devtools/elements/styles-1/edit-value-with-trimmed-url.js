// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that editing a CSS property value in the Styles pane restores the original, non-trimmed value text. Bug 107936. https://bugs.webkit.org/show_bug.cgi?id=107936\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="background: transparent url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAA5JREFUeNpiYGBgAAgwAAAEAAGbA+oJAAAAAElFTkSuQmCC) repeat-y 50% top">
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  function step1() {
    var treeElement = ElementsTestRunner.getElementStylePropertyTreeItem('background');
    TestRunner.addResult('Viewing \'background\' value in Styles:');
    TestRunner.addResult(treeElement.valueElement.textContent);

    treeElement.startEditingValue();
    TestRunner.addResult('Editing \'background\' value in Styles:');
    TestRunner.addResult(treeElement.valueElement.textContent);
    TestRunner.completeTest();
  }
})();
