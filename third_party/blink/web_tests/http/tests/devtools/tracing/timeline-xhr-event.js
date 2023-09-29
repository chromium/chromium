// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline events for XMLHttpReqeust\n`);
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
