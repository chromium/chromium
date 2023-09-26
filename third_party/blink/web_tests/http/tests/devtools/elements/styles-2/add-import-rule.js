// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that adding an @import with data URI does not lead to stylesheet collection crbug.com/644719\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <style>span { color: red }</style>
      <span id="styled-span"></span>
    `);

  var nodeId;
  var sheetId;

  ElementsTestRunner.selectNodeAndWaitForStyles('styled-span', nodeSelected);

  function nodeSelected(node) {
    nodeId = node.id;
    TestRunner.cssModel.getMatchedStyles(nodeId).then(matchedStylesBefore);
  }

  async function matchedStylesBefore(matchedResult) {
    sheetId = matchedResult.nodeStyles()[1].styleSheetId;
    TestRunner.addResult('\n== Matched rules before @import added ==\n');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.CSSAgent.setStyleSheetText(sheetId, '@import \'data:text/css,span{color:green}\';').then(sheetTextSet);
  }

  function sheetTextSet() {
    ElementsTestRunner.selectNodeAndWaitForStyles('styled-span', matchedStylesAfter);
  }

  async function matchedStylesAfter() {
    TestRunner.addResult('\n== Matched rules after @import added ==\n');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
