// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests the way service worker manager manages targets.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/';
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);

  SDK.TargetManager.TargetManager.instance().observeTargets({
    targetAdded: function(target) {
      TestRunner.addResult('Target added: ' + target.name() + '; type: ' + target.type());
      if (target.type() === SDK.Target.Type.ServiceWorker) {
        var serviceWorkerManager = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ServiceWorkerManager.ServiceWorkerManager);
        // Allow agents to do rountrips.
        TestRunner.deprecatedRunAfterPendingDispatches(function() {
          for (var registration of serviceWorkerManager.registrations().values())
            serviceWorkerManager.deleteRegistration(registration.id)
        });
      }
    },

    targetRemoved: function(target) {
      TestRunner.addResult('Target removed: ' + target.name() + '; type: ' + target.type());
      if (target.type() === SDK.Target.Type.ServiceWorker)
        setTimeout(TestRunner.completeTest);
    }
  });
})();
