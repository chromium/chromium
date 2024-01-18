// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that property value editing triggers style update in rendering engine.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="font-size: 19px"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', testEmulateKeypress);

  function testEmulateKeypress() {
    var treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('font-size');
    treeElement.startEditingValue();
    var selection = treeElement.valueElement.getComponentSelection();
    var range = selection.getRangeAt(0);
    var newRange = document.createRange();
    newRange.setStart(range.startContainer, 1);
    newRange.setEnd(range.startContainer, 1);
    selection.removeAllRanges();
    selection.addRange(newRange);
    // Use eventSender to emit "input" event.
    eventSender.keyDown('1');
    ElementsTestRunner.waitForStyleApplied(onStyleApplied);
  }

  function onStyleApplied() {
    ElementsTestRunner.nodeWithId('inspected', onNodeFound);
  }

  function onNodeFound(node) {
    TestRunner.cssModel.getInlineStyles(node.id).then(onInlineStyle);
  }

  function onInlineStyle(inlineStyleResult) {
    if (!inlineStyleResult || !inlineStyleResult.inlineStyle) {
      TestRunner.addResult('Failed to get inline styles.').TestRunner.completeTest();
      return;
    }
    TestRunner.addResult('font-size: ' + inlineStyleResult.inlineStyle.getPropertyValue('font-size'));
    TestRunner.completeTest();
  }
})();
