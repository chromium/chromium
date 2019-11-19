// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that keyframes are shown in styles pane inside display:none.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      @keyframes animName {
        from { color: green; }
        to { color: lime; }
      }
      #container {
        animation: animName 1000s;
        display: none;
      }
      #element {
        animation: inherit;
      }
    </style>
    <div id="container">
      <div id="element"></div>
    </div>
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('element', step1);

  function step1() {
    TestRunner.addResult('=== #element styles ===');
    ElementsTestRunner.dumpSelectedElementStyles(true);
    ElementsTestRunner.selectNodeAndWaitForStyles('container', step2);
  }

  function step2() {
    TestRunner.addResult('=== #container styles ===');
    ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
