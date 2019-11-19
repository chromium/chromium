// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks the RunMicrotasks event is emitted and nested into RunTask.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      var scriptUrl = "timeline-network-resource.js";

      function performActions()
      {
          function promiseResolved()
          {
              setTimeout(() => {}, 0);
          }

          return new Promise(fulfill => {
              var xhr = new XMLHttpRequest();
              xhr.onreadystatechange = () => xhr.readyState === 4 ? fulfill() : 0;
              xhr.onerror = fulfill;
              xhr.open("GET", "../resources/test.webp", true);
              xhr.send();
          }).then(promiseResolved);
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  const microTaskEvent = PerformanceTestRunner.mainTrackEvents().find(
      e => e.name === TimelineModel.TimelineModel.RecordType.RunMicrotasks);
  PerformanceTestRunner.printTraceEventProperties(microTaskEvent);
  const nested = PerformanceTestRunner.mainTrackEvents()
      .filter(e => e.name === TimelineModel.TimelineModel.RecordType.Task)
      .some(e => e.startTime <= microTaskEvent.startTime && microTaskEvent.endTime <= e.endTime);

  TestRunner.addResult(`Microtask event is nested into Task event: ${nested}`);

  TestRunner.completeTest();
})();
