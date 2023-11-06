// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CoverageTestRunner} from 'coverage_test_runner';

(async function() {
  TestRunner.addResult(`Tests the coverage list view after finishing recording in the Coverage view.\n`);
  await TestRunner.navigatePromise(TestRunner.url('resources/basic-coverage.html'));

  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.startCoverage();
  await CoverageTestRunner.pollCoverage();
  CoverageTestRunner.dumpCoverageListView();
  TestRunner.addResult('Reloading Page');
  await TestRunner.reloadPagePromise();
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  CoverageTestRunner.dumpCoverageListView();

  TestRunner.completeTest();
})();
