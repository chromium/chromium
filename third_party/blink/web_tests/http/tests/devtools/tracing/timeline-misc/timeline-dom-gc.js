// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests the Timeline API instrumentation of a DOM GC event\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.addScriptTag('../../../resources/run-after-layout-and-paint.js');

  await TestRunner.evaluateInPagePromise(`
        function produceGarbageForGCEvents()
        {
            window.gc();
            return new Promise(fulfill => runAfterLayoutAndPaint(fulfill));
        }
    `);

  await PerformanceTestRunner.invokeAsyncWithTimeline(
      'produceGarbageForGCEvents');

  const gcEvent = PerformanceTestRunner.findTimelineEvent(
      TimelineModel.TimelineModel.RecordType.GCCollectGarbage);
  if (gcEvent)
    TestRunner.addResult('SUCCESS: Found expected Blink GC event record');
  else
    TestRunner.addResult(`FAIL: Blink GC event record wasn't found`);
  TestRunner.completeTest();
})();
