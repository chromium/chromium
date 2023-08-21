// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
(async function() {
  TestRunner.addResult('The \'disable cache\' flag must affect on the certificate fetch request.\n');

  const outerUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/sxg-location.sxg';
  const certUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/127.0.0.1.sxg.pem.cbor';
  const innerUrl = 'https://127.0.0.1:8443/loading/sxg/resources/inner-url.html';

  await TestRunner.showPanel('network');
  await TestRunner.NetworkAgent.setCacheDisabled(false);

  // Load the test signed exchange first, to cache the certificate file.
  await TestRunner.addIframe(outerUrl);

  NetworkTestRunner.networkLog().reset();

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.addIframe(outerUrl + '?iframe-1');
  await addPrefetchAndWait(outerUrl + '?prefetch-1', innerUrl);

  await TestRunner.NetworkAgent.setCacheDisabled(false);
  await TestRunner.addIframe(outerUrl + '?iframe-2');
  await addPrefetchAndWait(outerUrl + '?prefetch-2', innerUrl);

  for (var request of NetworkTestRunner.networkLog().requests()) {
    if (request.url() != certUrl)
      continue;
    TestRunner.addResult(`* ${request.url()}`);
    TestRunner.addResult(`  cached: ${request.cached()}`);
  }
  TestRunner.completeTest();

  async function addPrefetchAndWait(prefetchUrl, waitUrl) {
    const promise = new Promise(resolve => {
        TestRunner.addSniffer(SDK.NetworkManager.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
        function loadingFinished(event) {
          var request = NetworkTestRunner.networkLog().requestByManagerAndId(
              TestRunner.networkManager, event.requestId);
          if (request.url() == waitUrl) {
            resolve();
          } else {
            TestRunner.addSniffer(SDK.NetworkManager.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
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
