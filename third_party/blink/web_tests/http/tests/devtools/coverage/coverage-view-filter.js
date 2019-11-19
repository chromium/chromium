// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the filter is properly applied to coverage list view.\n`);
  await TestRunner.loadModule('coverage_test_runner');

  await CoverageTestRunner.startCoverage(true);
  await TestRunner.navigatePromise(TestRunner.url('resources/basic-coverage.html'));
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();

  var coverageView = self.runtime.sharedInstance(Coverage.CoverageView);
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
    coverageView._filterInput.setValue(text);
    coverageView._filterInput._onChangeCallback();
    TestRunner.addResult(`Filter: '${text}'`);
  }
})();
