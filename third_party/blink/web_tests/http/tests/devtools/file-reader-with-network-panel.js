// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that FileReader's Blob request isn't shown in network panel.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.evaluateInPagePromise(`
      function readBlob()
      {
          var reader = new FileReader();
          reader.onloadend = function () {
              console.log('done');
          };
          reader.readAsArrayBuffer(new Blob([ 'test' ]));
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(messageAdded);
  TestRunner.evaluateInPage('readBlob();');

  async function messageAdded(payload) {
    var requests = NetworkTestRunner.networkRequests();
    TestRunner.addResult('requests in the network panel: ' + requests.length);
    TestRunner.assertTrue(requests.length == 0, 'Blob load request to the browser is shown in the network panel.');
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
