// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests fetch network resource type and content.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeFetch('resources/resource.php', {}, step2);

  async function step2() {
    var request1 = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult(request1.url());
    TestRunner.addResult('resource.type: ' + request1.resourceType());
    TestRunner.assertTrue(!request1.failed, 'Resource loading failed.');

    var { content, error, isEncoded } = await request1.requestContent();
    TestRunner.addResult('resource.content after requesting content: ' + content);
    TestRunner.completeTest();
  }

  function step3() {
  }
})();
