// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests content is available for imported resource request.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadData()
      {
          var link = document.createElement("link");
          link.rel = "import";
          link.href = "resources/imported.html";
          document.head.appendChild(link);
      }
  `);

  NetworkTestRunner.recordNetwork();
  TestRunner.evaluateInPage('loadData()', step2);

  function step2() {
    var request = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult(request.url());
    TestRunner.addResult('resource.type: ' + request.resourceType());
    TestRunner.assertTrue(!request.failed, 'Resource loading failed.');
    request.requestContent().then(step3);
  }

  function step3({ content, error, isEncoded }) {
    TestRunner.addResult('resource.content after requesting content: ' + content);
    TestRunner.completeTest();
  }
})();
