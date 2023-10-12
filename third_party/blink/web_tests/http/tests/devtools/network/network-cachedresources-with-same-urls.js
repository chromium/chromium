// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that when we load two different images from the same url (e.g. counters), their content is different in network panel as well.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadImages()
      {
          var image = new Image();
          image.onload = step2;
          image.src = "resources/resource.php?type=image&random=1";
          document.body.appendChild(image);
      }

      function step2()
      {
          var image = new Image();
          image.onload = imageLoaded;
          image.src = "resources/resource.php?type=image&random=1";
          document.body.appendChild(image);
      }

      function imageLoaded()
      {
          console.log("Done.");
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step2, true);
  NetworkTestRunner.recordNetwork();
  TestRunner.evaluateInPage('loadImages()');

  async function step2(msg) {
    // inspector-test.js appears in network panel occasionally in Safari on
    // Mac, so checking two last requests.
    var requests = NetworkTestRunner.networkRequests();
    var request1 = requests[requests.length - 2];
    var request2 = requests[requests.length - 1];

    var request1Content = await request1.requestContent();
    var request2Content = await request2.requestContent();

    TestRunner.addResult(request1.url());
    TestRunner.addResult(request2.url());
    TestRunner.assertTrue(request1Content.content !== request2Content.content);
    TestRunner.completeTest();
  }
})();
