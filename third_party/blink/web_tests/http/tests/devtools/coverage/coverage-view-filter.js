// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CoverageTestRunner} from 'coverage_test_runner';

import * as Coverage from 'devtools/panels/coverage/coverage.js';

(async function() {
  TestRunner.addResult(`Tests the filter is properly applied to coverage list view.\n`);
  await CoverageTestRunner.startCoverage(true);
  await TestRunner.navigatePromise(TestRunner.url('resources/basic-coverage.html'));
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();

  var coverageView = Coverage.CoverageView.CoverageView.instance();
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
