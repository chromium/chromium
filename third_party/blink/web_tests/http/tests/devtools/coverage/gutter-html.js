// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the coverage highlight in sources after the recording finishes.\n`);
  await TestRunner.loadModule('coverage_test_runner');
  await TestRunner.navigatePromise(TestRunner.url('resources/document.html'));

  await CoverageTestRunner.startCoverage();
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  await CoverageTestRunner.dumpDecorations('document.html');
  TestRunner.completeTest();
})();
