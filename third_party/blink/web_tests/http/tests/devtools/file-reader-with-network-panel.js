// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  // This await is necessary for evaluateInPagePromise to produce accurate line numbers.
  await TestRunner.addResult(`Tests that FileReader's Blob request isn't shown in network panel.\n`);
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
