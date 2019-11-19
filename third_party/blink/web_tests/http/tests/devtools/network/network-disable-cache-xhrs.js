// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests disabling cache from inspector.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?random=1&cached=1', true, firstXHRLoaded);

  function firstXHRLoaded() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?random=1&cached=1', true, step2);
  }

  function step2() {
    TestRunner.NetworkAgent.setCacheDisabled(true).then(step3);
  }

  function step3() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?random=1&cached=1', true, step4);
  }

  async function step4(msg) {
    // inspector-test.js appears in network panel occasionally in Safari on
    // Mac, so checking two last resources.
    var requests = NetworkTestRunner.networkRequests();
    var request1 = requests[requests.length - 3];
    var request2 = requests[requests.length - 2];
    var request3 = requests[requests.length - 1];

    var request1Content = await request1.requestContent();
    var request2Content = await request2.requestContent();
    var request3Content = await request3.requestContent();

    TestRunner.addResult(request1.url());
    TestRunner.addResult(request2.url());
    TestRunner.addResult(request3.url());
    TestRunner.assertTrue(request1Content.content === request2Content.content, 'First and second resources are equal');
    TestRunner.assertTrue(request2Content.content !== request3Content.content, 'Second and third resources differ');
    TestRunner.NetworkAgent.setCacheDisabled(false).then(step5);
  }

  function step5() {
    TestRunner.completeTest();
  }
})();
