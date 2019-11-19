// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that computing the initiator graph works for service worker request.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  let scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/';
  await ApplicationTestRunner.registerServiceWorker('../service-workers/resources/network-fetch-worker.js', scope);

  var requests = NetworkTestRunner.networkRequests();
  requests.forEach((request) => {
    TestRunner.addResult('\n' + request.url());
    var graph = SDK.networkLog.initiatorGraphForRequest(request);
    TestRunner.addResult('Initiators ' + Array.from(graph.initiators).map(request => request._url));
    TestRunner.addResult('Initiated ' + Array.from(graph.initiated.keys()).map(request => request._url));
  });

  TestRunner.completeTest();
})();
