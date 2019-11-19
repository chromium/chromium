// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that network request response view generates a view if no mime type is set.`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  SDK.multitargetNetworkManager.setBlockingEnabled(true);
  TestRunner.networkManager.addEventListener(
    SDK.NetworkManager.Events.RequestFinished, (event) => {
      const request = event.data;
      const networkRequests = NetworkTestRunner.networkRequests();
      const networkRequest = networkRequests[0];

      TestRunner.addResult('networkRequests.length: ' + networkRequests.length);
      TestRunner.addResult('request.url(): ' + request.url());
      TestRunner.addResult('request.failed: ' + request.failed);
      TestRunner.addResult('networkRequest.url(): ' + networkRequest.url());
      TestRunner.addResult('networkRequest.mimeType: ' + networkRequest.mimeType);

      const responseView = new Network.RequestResponseView(networkRequest);
      responseView.showPreview().then((emptyWidgetView) => {
        TestRunner.addResult(emptyWidgetView._textElement.textContent);
        TestRunner.completeTest();
      });
    }
  );

  SDK.multitargetNetworkManager.setBlockedPatterns([
    {url: '*', enabled: true}
  ]);

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeXHR('GET', 'http://localhost:8000');
})();
