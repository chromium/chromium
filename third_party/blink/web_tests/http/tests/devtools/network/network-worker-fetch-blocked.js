// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests blocking fetch in worker.\n`);
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(true);
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns([{url: 'resources/resource.php', enabled: true}]);

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
