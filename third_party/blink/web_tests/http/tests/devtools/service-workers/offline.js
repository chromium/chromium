// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests "Offline" checkbox does not crash. crbug.com/746220\n`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  const scriptURL = 'resources/service-worker-empty.js';
  const scope = 'resources/offline';

  // Register a service worker.
  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await ApplicationTestRunner.waitForActivated(scope);

  // Switch offline mode on.
  const oldNetwork = SDK.NetworkManager.MultitargetNetworkManager.instance().networkConditions();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(SDK.NetworkManager.OfflineConditions);

  // Switch offline mode off.
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(oldNetwork);

  // The test passes if it doesn't crash.
  TestRunner.completeTest();
})();
