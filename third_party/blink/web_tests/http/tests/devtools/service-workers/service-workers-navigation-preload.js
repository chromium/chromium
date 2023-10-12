// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests the navigation request related events are available in the DevTools\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  const scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/navigation-preload-worker.js';
  const scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/navigation-preload-scope.php';
  const preloadRequestIDs = {};

  function onRequestStarted(event) {
    const request = event.data.request;
    if (request.initiator().type != 'preload') {
      return;
    }
    preloadRequestIDs[request.requestId()] = true;
    TestRunner.addResult('onRequestStarted:');
    TestRunner.addResult('  url: ' + request.url());
  }

  function onResponseReceived(event) {
    const request = event.data.request;
    if (!preloadRequestIDs[request.requestId()]) {
      return;
    }
    TestRunner.addResult('onResponseReceived:');
    TestRunner.addResult('  statusCode: ' + request.statusCode);
    TestRunner.addResult('  timing available: ' + !!request.timing);
    request.requestHeaders().map(header => {
      if (header.name == 'Service-Worker-Navigation-Preload') {
        TestRunner.addResult('  requestHeaders[\'Service-Worker-Navigation-Preload\']: ' + header.value);
      }
    });
  }
  function onRequestFinished(event) {
    const request = event.data;
    if (!preloadRequestIDs[request.requestId()]) {
      return;
    }
    TestRunner.addResult('onRequestFinished:');
    if (request.localizedFailDescription) {
      TestRunner.addResult('  localizedFailDescription: ' + request.localizedFailDescription);
    }
  }

  SDK.TargetManager.TargetManager.instance().addModelListener(SDK.NetworkManager.NetworkManager, SDK.NetworkManager.Events.RequestStarted, onRequestStarted);
  SDK.TargetManager.TargetManager.instance().addModelListener(
      SDK.NetworkManager.NetworkManager, SDK.NetworkManager.Events.ResponseReceived, onResponseReceived);
  SDK.TargetManager.TargetManager.instance().addModelListener(SDK.NetworkManager.NetworkManager, SDK.NetworkManager.Events.RequestFinished, onRequestFinished);

  ApplicationTestRunner.registerServiceWorker(scriptURL, scope)
      .then(_ => ApplicationTestRunner.waitForActivated(scope))
      .then(_ => {
        TestRunner.addResult('-----------------');
        TestRunner.addResult('Loading an iframe.');
        return TestRunner.addIframe(scope);
      })
      .then(_ => {
        TestRunner.addResult('The iframe loaded.');
        TestRunner.addResult('-----------------');
        TestRunner.addResult('Loading another iframe.');
        return TestRunner.addIframe(scope + '?BrokenChunked');
      })
      .then(_ => {
        TestRunner.addResult('The iframe loaded.');
        TestRunner.addResult('-----------------');
        TestRunner.addResult('Loading another iframe.');
        return TestRunner.addIframe(scope + '?Redirect');
      })
      .then(_ => {
        TestRunner.addResult('The iframe loaded.');
        TestRunner.addResult('-----------------');
        TestRunner.addResult('Done');
        TestRunner.completeTest();
      });
})();
