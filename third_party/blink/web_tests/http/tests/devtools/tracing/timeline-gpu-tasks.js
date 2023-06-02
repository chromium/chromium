// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline events for GPUTask\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      async function performActions() {
        const gl = document.createElement('canvas').getContext('webgl');
        return gl.getParameter(gl.MAX_VIEWPORT_DIMS);
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  const tracingModel = PerformanceTestRunner.tracingModel();
  const hasGPUTasks = tracingModel.sortedProcesses().some(p => p.sortedThreads().some(t => t.events().some(
      event => event.name === TimelineModel.TimelineModel.RecordType.GPUTask)));
  TestRunner.addResult(`Found GPUTask events: ${hasGPUTasks}`);

  TestRunner.completeTest();
})();
