// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a network resource received data\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var image = new Image();
          var imagePromise = new Promise((fulfill) => image.onload = fulfill);
          // Use random urls to avoid caching.
          const random = Math.random();
          image.src = "resources/anImage.png?random=" + random;

          var scriptPromise = new Promise((fulfill) => window.timelineNetworkResourceEvaluated = fulfill);
          var script = document.createElement("script");
          script.src = "resources/timeline-network-resource.js?randome=" + random;
          document.body.appendChild(script);

          return Promise.all([imagePromise, scriptPromise]);
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  TestRunner.addResult('Script evaluated.');
  var event = PerformanceTestRunner.findTimelineEvent('ResourceReceivedData');
  if (event) {
    var data = event.args['data'];
    if (data && typeof data.encodedDataLength === 'number')
      TestRunner.addResult('Resource received data has length, test passed.');
  }
  TestRunner.completeTest();
})();
