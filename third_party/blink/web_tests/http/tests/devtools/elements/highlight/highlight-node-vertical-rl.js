// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <script src="/js-test-resources/ahem.js"></script>
    <style>
    body {
      margin: 0;
      font: 10px/10px Ahem;
    }
    #container {
      margin: 50px 60px 70px 80px;
      width: 300px;
      height: 300px;
      writing-mode: vertical-rl;
    }
    #child {
      padding: 50px 60px 70px 80px;
      width: 100px;
      height: 100px;
    }
    </style>
    <div id="container">
      <div id="child">
        <span id="span">ABCDEFG</span>
      </div>
    </div>
  `);

  await TestRunner.evaluateInPagePromise('');
  function dumpHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  }
  await dumpHighlight('container');
  await dumpHighlight('child');
  await dumpHighlight('span');

  let textNode = await ElementsTestRunner.findNodePromise(node => node.nodeValue() == 'ABCDEFG');
  let result = await TestRunner.OverlayAgent.getHighlightObjectForTest(textNode.id);
  TestRunner.addResult('TEXT' + JSON.stringify(result, null, 2));
  TestRunner.completeTest();
})();
