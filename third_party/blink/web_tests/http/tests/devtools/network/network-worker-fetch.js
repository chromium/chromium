// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests fetch in worker.\n`);
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();

  NetworkTestRunner.makeFetchInWorker('resource.php', {}, fetchCallback);

  function fetchCallback(result) {
    TestRunner.addResult('Fetch in worker result: ' + result);

    var requests = NetworkTestRunner.networkRequests();
    requests.forEach((request) => {
      TestRunner.addResult(request.url());
      TestRunner.addResult('resource.type: ' + request.resourceType());
      TestRunner.addResult('request.failed: ' + !!request.failed);
    });

    TestRunner.completeTest();
  }
})();
