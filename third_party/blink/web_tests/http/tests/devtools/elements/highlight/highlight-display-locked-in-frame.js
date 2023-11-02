// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests highlights for display locking in a frame.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <iframe id="container" style="content-visibility: hidden; contain-intrinsic-size: 10px;"
        srcdoc='<div id="child" style="width: 50px; height: 50px; background: blue">Text</div>'
      </iframe>
    `);

  function dumpChild() {
    ElementsTestRunner.dumpInspectorHighlightJSON('child', TestRunner.completeTest.bind(TestRunner));
  }

  function dumpContainerAndChild() {
    ElementsTestRunner.dumpInspectorHighlightJSON('container', dumpChild);
  }

  dumpContainerAndChild();
})();
