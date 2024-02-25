// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests ServiceWorkersView on resources panel.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope1 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/'; // with trailing '/'
  var scope2 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope2';  // without trailing '/'
  var step = 0;
  Application.ServiceWorkersView.setThrottleDisabledForDebugging(true);

  TestRunner.addSniffer(Application.ServiceWorkersView.ServiceWorkersView.prototype, 'updateRegistration', updateRegistration, true);
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
  Application.ResourcesPanel.ResourcesPanel.instance().sidebar.serviceWorkersTreeElement.select();
  TestRunner.addResult('Register ServiceWorker for scope1');
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope1);
})();
