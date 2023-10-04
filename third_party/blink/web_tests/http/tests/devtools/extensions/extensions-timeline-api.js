// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as Timeline from 'devtools/panels/timeline/timeline.js';

(async function() {
  await TestRunner.showPanel('timeline');

  TestRunner.enableTimelineExtensionAndStart = function(callback) {
    const traceProviders = Extensions.extensionServer.traceProviders();
    const provider = traceProviders[traceProviders.length - 1];
    const timelinePanel = Timeline.TimelinePanel.TimelinePanel.instance();
    const setting = Timeline.TimelinePanel.TimelinePanel.settingForTraceProvider(provider);
    setting.set(true);
    TestRunner.addResult(`Provider short display name: ${provider.shortDisplayName()}`);
    TestRunner.addResult(`Provider long display name: ${provider.longDisplayName()}`);
    PerformanceTestRunner.startTimeline().then(callback);
  }

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testTimeline(nextTest) {
      var session;
      var sessionTimeOffset;
      var startTime;

      function onRecordingStarted(s) {
        sessionTimeOffset = (Date.now() - performance.now()) * 1000;
        startTime = performance.now();
        output("traceProvider.onRecordingStarted fired.");
        output("TracingSession:");
        dumpObject(s);
        session = s;
       }

      function onRecordingStopped() {
        output("traceProvider.onRecordingStopped fired.");

        const endTime = performance.now();
        var pid = 1;
        var tid = 1;
        var step = (endTime - startTime) * 1000 / 10;
        var start = startTime * 1000;
        var data = { "traceEvents": [
            {"name": "Extension record X 1", "ts": start, "dur": step * 4, "ph": "X", "args": {},  "tid": tid, "pid": pid, "cat":"" },
            {"name": "Extension record X 2", "ts": start + step * 5, "dur": step * 5, "ph": "X", "args": {},  "tid": tid, "pid": pid, "cat":"" },
            {"name": "Extension record I 1", "ts": start + step * 5.5, "ph": "I", "args": {},  "tid": tid, "pid": pid, "cat":"" },
            {"name": "Extension record B+E", "ts": start + step * 6, "ph": "B", "args": {}, "tid": tid, "pid": pid, "cat":"" },
            {"name": "Extension record B+E", "ts": start + step * 10, "ph": "E", "args": {}, "tid": tid, "pid": pid, "cat":"" }
        ]};
        var url = "data:application/json," + escape(JSON.stringify(data));
        session.complete(url, sessionTimeOffset);
      }

      var traceProvider = webInspector.timeline.addTraceProvider("extension trace provider", "long extension name");
      output("TraceProvider:");
      dumpObject(traceProvider);
      traceProvider.onRecordingStarted.addListener(onRecordingStarted);
      traceProvider.onRecordingStopped.addListener(onRecordingStopped);
      extension_startTimeline(
          () => extension_stopTimeline(
              () => extension_dumpFlameChart(nextTest)));
    },

    function extension_startTimeline(callback) {
      evaluateOnFrontend("TestRunner.enableTimelineExtensionAndStart(reply);", callback);
    },

    function extension_stopTimeline(callback) {
      evaluateOnFrontend("PerformanceTestRunner.stopTimeline().then(reply);", callback);
    },

    function extension_dumpFlameChart(callback) {
      evaluateOnFrontend("PerformanceTestRunner.dumpTimelineFlameChart(['long extension name']); reply()", callback);
    },
  ]);
})();
