// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests service worker filters in network log.');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('network');

  const swActivatedPromise = new Promise(resolve => {
    TestRunner.mainTarget.registerServiceWorkerDispatcher({
      workerRegistrationUpdated: function(registrations) {},
      workerErrorReported: function(errorMessage) {},
      /**
       * @param {!Array<!Protocol.ServiceWorker.ServiceWorkerVersion>} versions
       */
      workerVersionUpdated: function(versions) {
        if (versions.length && versions[0].status === 'activated')
          resolve();
      }
    });
  });

  await TestRunner.navigatePromise(
      'resources/service-worker-flagged-requests.html');
  await swActivatedPromise;
  await TestRunner.reloadPagePromise();
  TestRunner.addResult('');

  await TestRunner.evaluateInPageAsync(
      `fetch('/devtools/network/resources/abe.png')`);
  await TestRunner.evaluateInPageAsync(
      `fetch('/devtools/network/resources/sw-cached-resource.txt')`);
  await TestRunner.evaluateInPageAsync(
      `fetch('/devtools/network/resources/sw-dropped-resource.txt')`);

  const filterChecks = [
    '',
    'is:service-worker-intercepted',
    '-is:service-worker-intercepted',
    'is:service-worker-initiated',
    '-is:service-worker-initiated',
  ];

  for (const filterText of filterChecks) {
    TestRunner.addResult(`filter text: '${filterText}'`);
    UI.panels.network._networkLogView._textFilterUI.setValue(filterText);
    UI.panels.network._networkLogView._filterChanged(/* event */ null);

    for (const node of UI.panels.network._networkLogView.flatNodesList()) {
      if (Network.NetworkLogView.isRequestFilteredOut(node))
        continue;

      const request = node.requestOrFirstKnownChildRequest();
      if (request.url().includes('service-worker-flagged-requests.js')) {
        // Skip requests to update the service worker that show up sometimes
        continue;
      }

      TestRunner.addResult('url: ' + request.url());
      TestRunner.addResult(
          '  fetchedViaServiceWorker: ' + request.fetchedViaServiceWorker);
      TestRunner.addResult(
          '  initiatedByServiceWorker: ' + request.initiatedByServiceWorker());
    }
    TestRunner.addResult('');
  }

  TestRunner.completeTest();
})();
