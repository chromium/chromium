// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test if coverage view also shows completly uncovered css files\n`);
  await TestRunner.loadLegacyModule('panels/coverage'); await TestRunner.loadTestModule('coverage_test_runner');
  await TestRunner.navigatePromise(
      TestRunner.url('resources/css-coverage-import.html'));

  await CoverageTestRunner.startCoverage(true);

  TestRunner.addResult(
      'Make sure all files are shown even when not covered so far');
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.addResult(
      'Make sure files are added as they are loaded on reload');
  await TestRunner.reloadPagePromise();
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();

  await CoverageTestRunner.stopCoverage();
  TestRunner.completeTest();
})();
