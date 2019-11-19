// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies the style info overlaid on an inspected node.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          color: red;
          background-color: blue;
          font-size: 20px;
      }
      </style>
      <div id="empty-div"></div>
      <div id="div-with-text">I have text</div>
    `);

  await ElementsTestRunner.dumpInspectorHighlightStyleJSON('empty-div');
  await ElementsTestRunner.dumpInspectorHighlightStyleJSON('div-with-text');
  TestRunner.completeTest();
})();
