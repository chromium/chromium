// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that dataReceived is called on NetworkDispatcher for all incoming data.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadIFrame()
      {
          var iframe = document.createElement("iframe");
          iframe.setAttribute("src", "resources/resource.php?size=50000");
          document.body.appendChild(iframe);
      }
  `);

  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'responseReceived', responseReceived);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFailed', loadingFailed);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'dataReceived', dataReceived);
  TestRunner.evaluateInPage('loadIFrame()');

  var encodedBytesReceived = 0;
  function responseReceived(requestId, loaderId, time, resourceType, response, frameId) {
    var request = SDK.NetworkLog.instance().requestByManagerAndId(TestRunner.networkManager, requestId);
    if (/resource\.php/.exec(request.url())) {
      TestRunner.addResult('Received response.');
      encodedBytesReceived += response.encodedDataLength;
    }
  }

  function loadingFinished(requestId, finishTime, encodedDataLength) {
    var request = SDK.NetworkLog.instance().requestByManagerAndId(TestRunner.networkManager, requestId);
    if (/resource\.php/.exec(request.url())) {
      TestRunner.assertEquals(encodedBytesReceived, encodedDataLength, 'Data length mismatch');
      TestRunner.addResult('SUCCESS');
      TestRunner.completeTest();
    }
  }

  function loadingFailed(requestId, time, localizedDescription, canceled) {
    var request = SDK.NetworkLog.instance().requestByManagerAndId(TestRunner.networkManager, requestId);
    if (/resource\.php/.exec(request.url())) {
      TestRunner.addResult('Loading failed!');
      TestRunner.completeTest();
    }
  }

  function dataReceived(requestId, time, dataLength, encodedDataLength) {
    TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'dataReceived', dataReceived);
    var request = SDK.NetworkLog.instance().requestByManagerAndId(TestRunner.networkManager, requestId);
    if (/resource\.php/.exec(request.url()))
      encodedBytesReceived += encodedDataLength;
  }
})();
