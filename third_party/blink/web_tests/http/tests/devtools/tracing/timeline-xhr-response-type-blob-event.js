// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for XMLHttpReqeust with responseType="blob"\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var xhr = new XMLHttpRequest();
          xhr.responseType = "blob";
          xhr.open("GET", "network/resources/resource.php", true);
          xhr.onload = function() { };  // This is necessary for XHRLoad event.
          // assigning callback to onload doesn't work here due to exception in responseXML handling for blob response type.
          xhr.onreadystatechange = done;
          function done()
          {
              if (xhr.readyState === 4)
                  callback();
          }
          xhr.send(null);
          return promise;
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  PerformanceTestRunner.printTimelineRecords('XHRReadyStateChange');
  PerformanceTestRunner.printTimelineRecords('XHRLoad');
  TestRunner.completeTest();
})();
