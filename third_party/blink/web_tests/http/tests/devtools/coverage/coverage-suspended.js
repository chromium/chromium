// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the coverage list view after suspending the coverage model.\n`);
  await TestRunner.loadModule('coverage_test_runner');
  await TestRunner.loadHTML(`
      <p class="class">
      </p>
    `);
  await TestRunner.addStylesheetTag('resources/highlight-in-source.css');

  await CoverageTestRunner.startCoverage(true);
  await CoverageTestRunner.suspendCoverageModel();
  await TestRunner.addScriptTag('resources/coverage.js');
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.resumeCoverageModel();
  await CoverageTestRunner.stopCoverage();
  TestRunner.addResult('Initial');
  CoverageTestRunner.dumpCoverageListView();

  await CoverageTestRunner.startCoverage(true);
  await CoverageTestRunner.suspendCoverageModel();
  await CoverageTestRunner.resumeCoverageModel();
  await CoverageTestRunner.stopCoverage();
  TestRunner.addResult('After second session');
  CoverageTestRunner.dumpCoverageListView();

  await CoverageTestRunner.suspendCoverageModel();
  await CoverageTestRunner.resumeCoverageModel();

  var coverageView = self.runtime.sharedInstance(Coverage.CoverageView);
  coverageView._clear();

  await CoverageTestRunner.startCoverage(true);
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  TestRunner.addResult('After clear');
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.completeTest();
})();
