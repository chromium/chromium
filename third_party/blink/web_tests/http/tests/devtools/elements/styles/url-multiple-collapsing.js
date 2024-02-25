// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that multiple URLs which are long are not squashed into a single URL. Bug 590172.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          background: -webkit-image-set(url("chrome-search://theme/IDR_THEME_NTP_BACKGROUND?pfncmbjabnpldlfbnmhnhblapoibfbei") 1x, url("chrome-search://theme/IDR_THEME_NTP_BACKGROUND@2x?pfncmbjabnpldlfbnmhnhblapoibfbei") 2x);
          color: red;
      }
      </style>
      <div id="inspected"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  function step1() {
    dumpDOM(ElementsTestRunner.getMatchedStylePropertyTreeItem('background'));
    TestRunner.completeTest();
  }

  function dumpDOM(treeItem) {
    var element = treeItem.listItemElement.getElementsByClassName('value')[0];
    var result = [];
    dumpNode(element, result);
    TestRunner.addResult(result.join(''));
  }

  function dumpNode(parentNode, result) {
    var childNodes = parentNode.childNodes;
    for (var i = 0; i < childNodes.length; ++i) {
      var node = childNodes[i];
      switch (node.nodeType) {
        case Node.ELEMENT_NODE:
          dumpNode(node, result);
          break;
        case Node.TEXT_NODE:
          result.push(node.nodeValue);
          break;
      }
    }
  }
})();
