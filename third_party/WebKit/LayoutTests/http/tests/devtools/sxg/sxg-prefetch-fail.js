// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the prefetch failed.\n');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  SDK.networkLog.reset();

  const promise = new Promise(resolve => {
    TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFailed', loadingFailed, true);
    function loadingFailed(requestId, time, localizedDescription, canceled) {
      var request = SDK.networkLog.requestByManagerAndId(TestRunner.networkManager, requestId);
      if (/sxg-invalid-validity-url\.sxg/.exec(request.url()))
        resolve();
    }
  });

  TestRunner.evaluateInPage(`
    (function () {
      const link = document.createElement('link');
      link.rel = 'prefetch';
      link.href = '/loading/sxg/resources/sxg-invalid-validity-url.sxg';
      document.body.appendChild(link);
    })()
  `);
  await promise;
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
