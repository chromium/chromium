// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests network panel timing.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadScripts()
      {
          // Wait 100 ms, then serve for 200ms file containing 300ms loop.
          var script = document.createElement("script");
          script.setAttribute("src", "resources/resource.php?type=js&wait=100&send=200&jsdelay=300&jscontent=resourceLoaded()");
          document.head.appendChild(script);

          // Wait 100 ms, then serve for 100ms and call console.log in content.
          script = document.createElement("script");
          script.setAttribute("src", "resources/resource.php?type=js&wait=100&send=100&jscontent=resourceLoaded()");
          document.head.appendChild(script);
      }

      var loadedResourceCount = 0;
      function resourceLoaded()
      {
          if (++loadedResourceCount === 2)
              console.log("Done.");
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage('loadScripts()');

  function step2() {
    // inspector-test.js appears in network panel occasionally in Safari on
    // Mac, so checking two last requests.
    var timerThreshold = 15;  // Windows timer accuracy.
    var requests = NetworkTestRunner.networkRequests();
    var requestsCount = requests.length;
    var request1 = requests[requestsCount - 2];
    TestRunner.addResult(request1.url());
    TestRunner.assertGreaterOrEqual(request1.latency * 1000, 100 - timerThreshold, 'Latency of the first resource');
    TestRunner.assertGreaterOrEqual(request1.duration * 1000, 300 - timerThreshold, 'Duration of the first resource');

    var request2 = requests[requestsCount - 1];
    TestRunner.addResult(request2.url());
    TestRunner.assertGreaterOrEqual(request2.latency * 1000, 100 - timerThreshold, 'Latency of the second resource');
    TestRunner.assertGreaterOrEqual(request2.duration * 1000, 100 - timerThreshold, 'Duration of the second resource');
    TestRunner.completeTest();
  }
})();
