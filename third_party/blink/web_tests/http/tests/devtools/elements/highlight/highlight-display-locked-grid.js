// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests highlights for display locking.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
        #container {
          display: grid;
          grid-template-rows: 1fr 1fr;
          grid-template-columns: 1fr 1fr;
        }
      </style>
      <div id="container" style="content-visibility: hidden; contain-intrinsic-size: 10px;">
        <div id="child" style="width: 50px; height: 50px; background: blue">Text</div>
      </div>
    `);

  function dumpChild() {
    ElementsTestRunner.dumpInspectorHighlightJSON('child', TestRunner.completeTest.bind(TestRunner));
  }

  function dumpContainerAndChild() {
    ElementsTestRunner.dumpInspectorHighlightJSON('container', dumpChild);
  }

  dumpContainerAndChild();
})();
