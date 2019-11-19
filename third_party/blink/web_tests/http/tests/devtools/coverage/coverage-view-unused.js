// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test if coverage view also shows completly uncovered css files\n`);
  await TestRunner.loadModule('coverage_test_runner');
  await TestRunner.navigatePromise(TestRunner.url('resources/unused-css-coverage.html'));

  await CoverageTestRunner.startCoverage(true);

  TestRunner.addResult('Make sure all files are shown even when not covered so far');
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.addResult('Make sure files are added as they are loaded on reload');
  await TestRunner.reloadPagePromise();
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.addResult('Make sure files are added when they are not part of the initial load');
  await TestRunner.evaluateInPagePromise('appendStylesheet()');
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.addResult('Make sure coverage gets updated if anything changes');
  await TestRunner.evaluateInPagePromise('appendElement()');
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();

  await CoverageTestRunner.stopCoverage();
  TestRunner.completeTest();
})();
