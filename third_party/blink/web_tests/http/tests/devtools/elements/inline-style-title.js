// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that external change of inline style element updates its title.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style id="inline-style">
          div { color: red }
      </style>
    `);
  await TestRunner.evaluateInPagePromise(`
      function updateStyleText()
      {
          document.querySelector("#inline-style").textContent = "span { border: 1px solid black }";
      }
  `);

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  ElementsTestRunner.nodeWithId('inline-style', onInlineStyleQueried);

  var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  var treeElement;

  function onInlineStyleQueried(node) {
    if (!node) {
      TestRunner.addResult('Didn\'t find node with given ID');
      TestRunner.completeTest();
      return;
    }
    treeElement = treeOutline.findTreeElement(node);
    TestRunner.addResult('=== initial inline style text ===');
    TestRunner.addResult(treeElement.title.textContent);
    TestRunner.evaluateInPage('updateStyleText()', onStyleUpdated);
  }

  function onStyleUpdated() {
    ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();
    TestRunner.addResult('=== inline style text after change ===');
    TestRunner.addResult(treeElement.title.textContent);
    TestRunner.completeTest();
  }
})();
