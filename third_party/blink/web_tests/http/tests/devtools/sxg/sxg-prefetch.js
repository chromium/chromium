// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the prefetch succeeded.\n');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('network');
  NetworkTestRunner.networkLog().reset();

  const promise = new Promise(resolve => {
    TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished, true);
    function loadingFinished(requestId, finishTime, encodedDataLength) {
      var request = NetworkTestRunner.networkLog().requestByManagerAndId(
          TestRunner.networkManager, requestId);
      if (/inner-url\.html/.exec(request.url()))
        resolve();
    }
  });

  TestRunner.evaluateInPage(`
    (function () {
      const link = document.createElement('link');
      link.rel = 'prefetch';
      link.href = '/loading/sxg/resources/sxg-location.sxg';
      document.body.appendChild(link);
    })()
  `);
  await promise;
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
