// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CoverageTestRunner} from 'coverage_test_runner';

import * as Coverage from 'devtools/panels/coverage/coverage.js';

(async function() {
  TestRunner.addResult(`Tests the if events are getting emitted when coverage changes.\n`);


  TestRunner.addResult('Should have coverage information even when not covered yet');
  await CoverageTestRunner.startCoverage(true);
  await TestRunner.addScriptTag('resources/coverage.js');
  await CoverageTestRunner.pollCoverage();
  const coverageModel = CoverageTestRunner.getCoverageModel();
  const coverageInfo = coverageModel.getCoverageForUrl(TestRunner.url('resources/coverage.js'));

  TestRunner.addResult('Coverage info found.');


  TestRunner.addResult('Coverage should emit an event whenever it changes');

  const sizesUpdatedPromise = coverageInfo.once(Coverage.CoverageModel.URLCoverageInfo.Events.SizesChanged);
  await TestRunner.evaluateInPagePromise('performActions()');
  await sizesUpdatedPromise; // Wait for the event to be triggered.

  TestRunner.addResult('Event got emitted.');

  await CoverageTestRunner.stopCoverage();

  TestRunner.addResult('Coverage should emit an event when it gets reset');
  const coverageResetPromise = coverageModel.once(Coverage.CoverageModel.Events.CoverageReset);
  coverageModel.reset();
  await coverageResetPromise; // Wait for the event to be triggered.

  TestRunner.addResult('Event got emitted.');

  TestRunner.completeTest();
})();
