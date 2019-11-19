// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests empty xhr content is correctly loaded in inspector. https://bugs.webkit.org/show_bug.cgi?id=79026`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  function dumpRequest(request, callback) {
    if (!request)
      return callback();
    TestRunner.addResult(request.url());

    function contentLoaded({ content, error, isEncoded }) {
      TestRunner.addResult('resource.content: ' + content);
      callback();
    }

    request.requestContent().then(contentLoaded);
  }

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeSimpleXHR('GET', 'resources/empty.html?sync', false, firstXHRLoaded);

  function firstXHRLoaded() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/empty.html?async', true, step2);
  }

  function step2() {
    var requests = NetworkTestRunner.networkRequests();
    var request = requests[requests.length - 2];
    dumpRequest(request, step3);
  }

  function step3() {
    var requests = NetworkTestRunner.networkRequests();
    var request = requests[requests.length - 1];
    dumpRequest(request, step4);
  }

  function step4() {
    TestRunner.completeTest();
  }
})();
