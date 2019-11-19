// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the coverage highlight in sources after the recording finishes.\n`);
  await TestRunner.loadModule('coverage_test_runner');
  await TestRunner.addScriptTag('resources/coverage.js');

  await CoverageTestRunner.startCoverage(true);
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  await CoverageTestRunner.dumpDecorations('coverage.js');
  TestRunner.completeTest();
})();
