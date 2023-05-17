// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Test that parallel fetches in worker should not cause crash.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function makeFetchesInWorker(urls)
      {
          return new Promise((resolve) => {
              var worker = new Worker('/devtools/network/resources/fetch-parallel-worker.js');
              worker.onmessage = (event) => {
                  resolve(JSON.stringify(event.data));
              };
              worker.postMessage(urls);
          });
      }
  `);

  NetworkTestRunner.recordNetwork();

  TestRunner.callFunctionInPageAsync('makeFetchesInWorker', [['./resource.php?1', './resource.php?2']])
      .then((result) => {
        TestRunner.addResult('Parallel fetch in worker result: ' + result);
        var requests = NetworkTestRunner.networkRequests();
        requests.forEach((request) => {
          TestRunner.addResult(request.url());
          TestRunner.addResult('resource.type: ' + request.resourceType());
          TestRunner.addResult('request.failed: ' + !!request.failed);
        });
        TestRunner.completeTest();
      });
})();
