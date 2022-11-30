// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the filter is properly applied to coverage list view.\n`);
  await TestRunner.loadLegacyModule('panels/coverage'); await TestRunner.loadTestModule('coverage_test_runner');

  await CoverageTestRunner.startCoverage(false);
  await TestRunner.navigatePromise(TestRunner.url('resources/basic-coverage.html'));
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();

  var coverageView = Coverage.CoverageView.instance();
  setFilter('devtools');
  CoverageTestRunner.dumpCoverageListView();
  setFilter('CES/COV');
  CoverageTestRunner.dumpCoverageListView();
  setFilter('no pasaran');
  CoverageTestRunner.dumpCoverageListView();
  setFilter('');
  CoverageTestRunner.dumpCoverageListView();
  TestRunner.completeTest();

  function setFilter(text) {
    coverageView.filterInput.setValue(text);
    coverageView.filterInput.onChangeCallback();
    TestRunner.addResult(`Filter: '${text}'`);
  }
})();
