// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests XHR network resource type and content for asynchronous requests. Bug 61205\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php', true, step2);

  function step2() {
    var request1 = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult(request1.url());
    TestRunner.addResult('resource.type: ' + request1.resourceType());
    TestRunner.assertTrue(!request1.failed, 'Resource loading failed.');
    request1.requestContent().then(step3);
  }

  function step3({ content, error, isEncoded }) {
    TestRunner.addResult('resource.content after requesting content: ' + content);
    ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
