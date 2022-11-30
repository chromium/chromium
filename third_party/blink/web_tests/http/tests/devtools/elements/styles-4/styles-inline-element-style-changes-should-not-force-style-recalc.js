// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that inspector doesn't force styles recalc on operations with inline element styles that result in no changes.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="testDiv" style="color: green">testDiv</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var testDiv = document.querySelector("#testDiv");
          for (var i = 0; i < 20; ++i)
              testDiv.style.visibility = "";
      }
  `);

  UI.context.setFlavor(Timeline.TimelinePanel, UI.panels.timeline);
  PerformanceTestRunner.performActionsAndPrint('performActions()', 'RecalculateStyles');
})();
