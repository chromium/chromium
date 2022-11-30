// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that ping request response is recorded.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function sendBeacon()
      {
          console.log("Beacon sent: " + navigator.sendBeacon("resources/empty.html", "foo"));
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage('sendBeacon()');

  function step2() {
    NetworkTestRunner.networkRequests().pop().requestContent().then(step3);
  }

  async function step3() {
    var request = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult('URL: ' + request.url());
    TestRunner.addResult('Finished: ' + request.finished);
    TestRunner.addResult('Cached: ' + request.cached());
    TestRunner.addResult('Method: ' + request.requestMethod);
    TestRunner.addResult('Status: ' + request.statusCode + ' ' + request.statusText);
    TestRunner.addResult('Has raw request headers: ' + (typeof request.requestHeadersText() === 'string'));
    TestRunner.addResult('Has raw response headers: ' + (typeof request.responseHeadersText === 'string'));
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
