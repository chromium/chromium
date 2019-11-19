// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests content is available for failed image request.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadData()
      {
          var image = new Image();
          image.src = "resources/404.php";
          image.onerror = resourceLoaded;
      }

      function resourceLoaded()
      {
          console.log("Done.");
      }
  `);

  NetworkTestRunner.recordNetwork();
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage('loadData()');

  function step2() {
    var request1 = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult(request1.url());
    TestRunner.addResult('resource.type: ' + request1.resourceType());
    TestRunner.assertTrue(!request1.failed, 'Resource loading failed.');
    request1.requestContent().then(step3);
  }

  async function step3() {
    var requests = NetworkTestRunner.networkRequests();
    requests.sort(function(a, b) {
      return a.url().localeCompare(b.url());
    });
    TestRunner.addResult('resources count = ' + requests.length);
    for (i = 0; i < requests.length; i++) {
      TestRunner.addResult(requests[i].url());
      var { content, error, isEncoded } = await requests[i].requestContent();
      TestRunner.addResult('resource.content after requesting content: ' + content);
    }

    TestRunner.completeTest();
  }
})();
