// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that url(...) with space-delimited color names as filename segments do not contain color swatches. Bug 106770. Also tests that CSS variables such as var(--blue) do not contain color swatches. Bug 595231.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          background-image: url("red green blue.jpg");
          color: red;
          --blue: blue;
          border-color: var(--blue);
      }
      </style>
      <div id="inspected"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  function step1() {
    dumpDOM(ElementsTestRunner.getMatchedStylePropertyTreeItem('background-image'));
    dumpDOM(ElementsTestRunner.getMatchedStylePropertyTreeItem('color'));
    dumpDOM(ElementsTestRunner.getMatchedStylePropertyTreeItem('border-color'));
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
          if (node.getAttribute('is') === 'color-swatch') {
            result.push('[] ' + node.textContent);
          } else if (
              node.tagName.toLowerCase() === 'devtools-css-var-swatch' &&
              node.shadowRoot.querySelector('.color-swatch-inner')) {
            result.push('[] ' + node.textContent);
          } else {
            dumpNode(node, result);
          }
          break;
        case Node.TEXT_NODE:
          result.push(node.nodeValue);
          break;
      }
    }
  }
})();
