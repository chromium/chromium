// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verifies that network request response view generates a view if no mime type is set.`);
  await TestRunner.showPanel('network');

  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(true);
  TestRunner.networkManager.addEventListener(
    SDK.NetworkManager.Events.RequestFinished, (event) => {
      const request = event.data;
      const networkRequests =
          NetworkTestRunner.networkRequests().filter((e, i, a) => i % 2 == 0);
      const networkRequest = networkRequests[0];

      TestRunner.addResult('networkRequests.length: ' + networkRequests.length);
      TestRunner.addResult('request.url(): ' + request.url());
      TestRunner.addResult('request.failed: ' + request.failed);
      TestRunner.addResult('networkRequest.url(): ' + networkRequest.url());
      TestRunner.addResult('networkRequest.mimeType: ' + networkRequest.mimeType);

      const responseView = new Network.RequestResponseView.RequestResponseView(networkRequest);
      responseView.showPreview().then((emptyWidgetView) => {
        TestRunner.addResult(emptyWidgetView.textElement.textContent);
        TestRunner.completeTest();
      });
    }
  );

  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns([
    {url: '*', enabled: true}
  ]);

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeXHR('GET', 'http://localhost:8000');
})();
