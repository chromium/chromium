// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Trivial use of inspector frontend tests\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  /* This test seems silly, but originally it tickled bug 31080 */
  await PerformanceTestRunner.startTimeline();
  TestRunner.addResult('Timeline started');
  TestRunner.completeTest();
})();
