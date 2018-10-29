// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('The \'disable cache\' flag must affect on the certificate fetch request.\n');

  const outerUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/sxg-location.sxg';
  const certUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/127.0.0.1.sxg.pem.cbor';
  const innerUrl = 'https://127.0.0.1:8443/loading/sxg/resources/inner-url.html';

  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.NetworkAgent.setCacheDisabled(false);

  // Load the test signed exchange first, to cache the certificate file.
  await TestRunner.addIframe(outerUrl);

  SDK.networkLog.reset();

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.addIframe(outerUrl + '?iframe-1');
  await addPrefetchAndWait(outerUrl + '?prefetch-1', innerUrl);

  await TestRunner.NetworkAgent.setCacheDisabled(false);
  await TestRunner.addIframe(outerUrl + '?iframe-2');
  await addPrefetchAndWait(outerUrl + '?prefetch-2', innerUrl);

  for (var request of SDK.networkLog.requests()) {
    if (request.url() != certUrl)
      continue;
    TestRunner.addResult(`* ${request.url()}`);
    TestRunner.addResult(`  cached: ${request.cached()}`);
  }
  TestRunner.completeTest();

  async function addPrefetchAndWait(prefetchUrl, waitUrl) {
    const promise = new Promise(resolve => {
        TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
        function loadingFinished(requestId, finishTime, encodedDataLength) {
          var request = SDK.networkLog.requestByManagerAndId(TestRunner.networkManager, requestId);
          if (request.url() == waitUrl) {
            resolve();
          } else {
            TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
          }
        }
      });
    TestRunner.evaluateInPage(`
          (function () {
            const link = document.createElement('link');
            link.rel = 'prefetch';
            link.href = '${prefetchUrl}';
            document.body.appendChild(link);
          })()
        `);
    await promise;
  }
})();
