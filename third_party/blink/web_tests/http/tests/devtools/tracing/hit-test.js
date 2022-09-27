// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests instrumentation for Timeline HitTest event.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`<p>A text</p>`);
  await TestRunner.evaluateInPagePromise(`
    function performActions() {
      var e = document.elementFromPoint(10, 10);
    }`);
  await PerformanceTestRunner.performActionsAndPrint('performActions()', 'HitTest');
})();
