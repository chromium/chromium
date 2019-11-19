// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the coverage export functionality and format.\n`);
  await TestRunner.loadModule('coverage_test_runner');
  await TestRunner.navigatePromise(TestRunner.url('resources/basic-coverage.html'));

  await CoverageTestRunner.startCoverage(true);
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  const report = JSON.parse(await CoverageTestRunner.exportReport());
  for (const entry of report) {
    TestRunner.addResult('\n\nFile: ' + entry.url);
    for (const range of entry.ranges) {
      TestRunner.addResult(`\nUsage: [${range.start}, ${range.end}]`);
      TestRunner.addResult(entry.text.substring(range.start, range.end).trim());
    }
  }
  TestRunner.completeTest();
})();
