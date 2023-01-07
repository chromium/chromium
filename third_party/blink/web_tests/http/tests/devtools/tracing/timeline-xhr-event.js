// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for XMLHttpReqeust\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "network/resources/resource.php", true);
          xhr.onload = callback;  // This is necessary for XHRLoad event.
          xhr.onreadystatechange = function () { };  // This is necessary for XHRReadyStateChange event.
          xhr.send(null);
          return promise;
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  await PerformanceTestRunner.printTimelineRecordsWithDetails('XHRReadyStateChange');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('XHRLoad');
  TestRunner.completeTest();
})();
