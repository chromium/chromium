// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests ServiceWorkersView on resources panel.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope1 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/'; // with trailing '/'
  var scope2 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope2';  // without trailing '/'
  var step = 0;
  Resources.ServiceWorkersView._noThrottle = true;

  TestRunner.addSniffer(Resources.ServiceWorkersView.prototype, '_updateRegistration', updateRegistration, true);
  function updateRegistration(registration) {
    for (var version of registration.versions.values()) {
      if (step == 0 && registration.scopeURL == scope1 && version.isActivated() && version.isRunning()) {
        ++step;
        TestRunner.addResult(ApplicationTestRunner.dumpServiceWorkersView());
        TestRunner.addResult('Register ServiceWorker for scope2');
        ApplicationTestRunner.registerServiceWorker(scriptURL, scope2);
      } else if (step == 1 && registration.scopeURL == scope2 && version.isActivated() && version.isRunning()) {
        ++step;
        TestRunner.addResult(ApplicationTestRunner.dumpServiceWorkersView());
        TestRunner.addResult('Unregister ServiceWorker for scope1');
        ApplicationTestRunner.unregisterServiceWorker(scope1);
      } else if (step == 2 && registration.scopeURL == scope1 && version.isRedundant() && version.isRunning()) {
        ++step;
        TestRunner.addResult(ApplicationTestRunner.dumpServiceWorkersView());
        TestRunner.addResult('Unregister ServiceWorker for scope2');
        ApplicationTestRunner.unregisterServiceWorker(scope2);
      } else if (step == 3 && registration.scopeURL == scope2 && version.isRedundant()) {
        ++step;
        ApplicationTestRunner.deleteServiceWorkerRegistration(scope1);
        ApplicationTestRunner.deleteServiceWorkerRegistration(scope2);
        TestRunner.completeTest();
      }
    }
  }

  TestRunner.addResult('Select ServiceWorkers tree element.');
  UI.panels.resources._sidebar.serviceWorkersTreeElement.select();
  TestRunner.addResult('Register ServiceWorker for scope1');
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope1);
})();
