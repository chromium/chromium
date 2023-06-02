// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests js cpu profile in timeline.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      // Save references to the worker objects to make sure they are not GC'ed.
      var worker1;
      var worker2;

      // The function ensures call stack for the current frame is captured.
      function makeDOMEvent()
      {
          var e = document.createElement("div");
          e.innerHTML = "<p>test</p>";
          return e.lastChild.textContents;
      }

      function startFirstWorker()
      {
          worker1 = new Worker("resources/worker.js");
          worker1.postMessage("");
          return new Promise((fulfill) => worker1.onmessage = fulfill);
      }

      function startSecondWorker()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          // Make sure there is at least one trace event recorded for the first worker.
          worker1.onmessage = gotResponseFromFirstWorker;
          worker1.postMessage("");
          makeDOMEvent();

          function gotResponseFromFirstWorker()
          {
              worker1.onmessage = null;

              worker2 = new Worker("resources/worker.js");
              worker2.postMessage("");
              worker2.onmessage = function(event)
              {
                  worker2.onmessage = null;
                  makeDOMEvent();
                  callback();
              }
          }
          return promise;
      }
  `);

  await TestRunner.evaluateInPageAsync(`startFirstWorker()`);
  PerformanceTestRunner.invokeWithTracing(
      'startSecondWorker', processTracingEvents, 'disabled-by-default-devtools.timeline.stack', true);

  function processTracingEvents() {
    processTrack(new Set(['startSecondWorker', 'worker2.onmessage']), PerformanceTestRunner.mainTrack());
    for (var track of PerformanceTestRunner.timelineModel().tracks()) {
      if (track.type !== TimelineModel.TimelineModel.TrackType.Worker)
        continue;
      processTrack(new Set(['Function Call']), track);
    }
    TestRunner.completeTest();
  }

  function processTrack(expectedEvents, track) {
    TestRunner.addResult(track.name);
    var missingEvents = track.events.reduce(processEvent, expectedEvents);
    if (missingEvents.size) {
      TestRunner.addResult('FAIL: missing events:');
      missingEvents.forEach(TestRunner.addResult);
    }
  }

  function processEvent(expectedEvents, event) {
    expectedEvents.delete(Timeline.TimelineUIUtils.eventTitle(event));
    return expectedEvents;
  }
})();
