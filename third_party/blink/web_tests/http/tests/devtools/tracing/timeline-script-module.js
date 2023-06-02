// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline events for module compile & evaluate.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
        var script = document.createElement('script');
        script.type = 'module';
        script.text = 'window.finishTest()';
        document.body.appendChild(script);
        return new Promise(resolve => window.finishTest = resolve);
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  const events = new Set([TimelineModel.TimelineModel.RecordType.CompileModule, TimelineModel.TimelineModel.RecordType.EvaluateModule]);
  const tracingModel = PerformanceTestRunner.tracingModel();

  const eventsToPrint = [];
  tracingModel.sortedProcesses().forEach(p => p.sortedThreads().forEach(t =>
      eventsToPrint.push(...t.events().filter(event => events.has(event.name)))));
  for (const event of eventsToPrint) {
    await PerformanceTestRunner.printTraceEventPropertiesWithDetails(event);
  }

  TestRunner.completeTest();
})();
