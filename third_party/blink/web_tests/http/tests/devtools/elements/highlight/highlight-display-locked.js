// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests highlights for display locking.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" renderSubtree="invisible skip-activation" style="intrinsic-size: 10px;">
        <div id="child" style="width: 50px; height: 50px;"></div>
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
