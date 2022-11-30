// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the coverage list view after finishing recording in the Coverage view.\n`);
  await TestRunner.loadLegacyModule('panels/coverage'); await TestRunner.loadTestModule('coverage_test_runner');
  await TestRunner.navigatePromise(TestRunner.url('resources/basic-coverage.html'));

  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.startCoverage(true);
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();
  TestRunner.addResult('Reloading Page');
  await TestRunner.reloadPagePromise();
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.completeTest();
})();
