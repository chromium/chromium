// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as TimelineModel from 'devtools/models/timeline_model/timeline_model.js';

(async function() {
  TestRunner.addResult(`Tests V8 cache information of Service Worker Cache Storage in timeline\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function registerServiceWorkerAndwaitForActivated() {
        const script = 'resources/v8-cache-worker.js';
        const scope = 'resources/v8-cache-iframe.html';
        return registerServiceWorker(script, scope)
          .then(() => waitForActivated(scope));
      }
      function loadScript() {
        const url = '/devtools/resources/v8-cache-script.js';
        const frameId = 'frame_id';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url)
          .then(() => iframeWindow.loadScript(url));
      }
  `);

  const scope = 'resources/v8-cache-iframe.html';
  const frameId = 'frame_id';

  await PerformanceTestRunner.invokeAsyncWithTimeline('registerServiceWorkerAndwaitForActivated');
  TestRunner.addResult('--- Trace events while installing -------------');
  await PerformanceTestRunner.printTimelineRecordsWithDetails(
    TimelineModel.TimelineModel.RecordType.CompileScript,
    TimelineModel.TimelineModel.RecordType.CacheScript);
  TestRunner.addResult('-----------------------------------------------');
  await ApplicationTestRunner.waitForActivated(scope);
  await TestRunner.addIframe(scope, {id: frameId});
  await PerformanceTestRunner.invokeAsyncWithTimeline('loadScript');
  TestRunner.addResult('--- Trace events while executing scripts ------');
  await PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileScript,
      TimelineModel.TimelineModel.RecordType.CacheScript);
  TestRunner.addResult('-----------------------------------------------');
  TestRunner.completeTest();
})();
