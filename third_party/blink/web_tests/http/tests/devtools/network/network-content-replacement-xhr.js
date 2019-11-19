// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests NetworkResourcesData logic for XHR content replacement.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  TestRunner.NetworkAgent.setDataSizeLimitsForTest(300, 200);
  // Here we test replacement logic. We save first two resources content,
  // discard third resource content once we see its size exceeds limit,
  // and finally replace first resource content with the last resource content.

  NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?size=200', false, xhrLoaded1);

  function xhrLoaded1() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?size=100', false, xhrLoaded2);
  }

  function xhrLoaded2() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?size=201', false, xhrLoaded3);
  }

  function xhrLoaded3() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php?size=100', false, step1);
  }

  function dumpRequest(request, callback) {
    if (!request)
      return callback();
    TestRunner.addResult(request.url());

    async function contentLoaded({ content, error, isEncoded }) {
      TestRunner.addResult('resource.content: ' + content);
      callback();
    }

    request.requestContent().then(contentLoaded);
  }

  function step1() {
    var requests = NetworkTestRunner.networkRequests();
    dumpRequest(requests[requests.length - 4], step2);
  }

  function step2() {
    var requests = NetworkTestRunner.networkRequests();
    dumpRequest(requests[requests.length - 3], step3);
  }

  function step3() {
    var requests = NetworkTestRunner.networkRequests();
    dumpRequest(requests[requests.length - 2], step4);
  }

  function step4() {
    var requests = NetworkTestRunner.networkRequests();
    dumpRequest(requests[requests.length - 1], step5);
  }

  function step5() {
    TestRunner.completeTest();
  }
})();
