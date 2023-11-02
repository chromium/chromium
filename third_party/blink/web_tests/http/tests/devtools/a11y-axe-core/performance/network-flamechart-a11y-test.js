// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Testing a11y in performance panel - network flamechart.');

  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.runPerfTraceWithReload();
  const networkFlameChartElement = await PerformanceTestRunner.getNetworkFlameChartElement();
  await AxeCoreTestRunner.runValidation(networkFlameChartElement);

  TestRunner.completeTest();
})();
