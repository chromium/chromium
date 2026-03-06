// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that loading the same image url twice in the same document reuses the cached resource (available-image reuse).\n`);
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
    var requests = NetworkTestRunner.networkRequests();
    // Filter to only the resource.php image requests.
    var imageRequests = requests.filter(r => r.url().includes('resource.php'));

    TestRunner.addResult('Number of resource.php requests: ' + imageRequests.length);
    TestRunner.assertTrue(imageRequests.length === 1, 'Expected exactly one network request due to available-image reuse');
    TestRunner.addResult(imageRequests[0].url());
    TestRunner.completeTest();
  }
})();
