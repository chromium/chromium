// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Common from 'devtools/core/common/common.js';

// From `forceSpeculationRules()` in
// third_party/blink/web_tests/external/wpt/speculation-rules/prefetch/resources/utils.sub.js
async function forceSpeculationRules(url) {
  await TestRunner.callFunctionInPageAsync(`
    (() => {
      const script = document.createElement("script");
      script.type = "speculationrules";
      script.textContent = JSON.stringify({
        "prefetch": [
          {
            source: "list",
            urls: ["${url}"]
          }
        ]
      });
      document.body.appendChild(script);
    })`);
  await new Promise(resolve => setTimeout(resolve, 2000));
}

async function testNavigation(url) {
  await TestRunner.evaluateInPagePromise(`location.href = "${url}";`);
  await new Promise(resolve => setTimeout(resolve, 2000));

  const t = await TestRunner.callFunctionInPageAsync(
      '(() => { return document.body.textContent; })');
  TestRunner.addResult(t);
}

(async function() {
  TestRunner.addResult(`Tests "Bypass for network" checkbox shouldn't be ` +
      `served by ServiceWorker-controlled speculation rules prefetches.\n`);
  // Note: every test that uses a storage API must manually clean-up state from
  // previous tests.
  await ApplicationTestRunner.resetState();

  const scriptURL = 'resources/service-workers-bypass-for-network-navigation-worker.js';
  const scope = 'resources/service-workers-bypass-for-network-navigation-iframe.html';

  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await ApplicationTestRunner.waitForActivated(scope);

  // Prefetch the URL (served by the ServiceWorker).
  // TODO(https://crbug.com/438478667): Currently the 'bypass-service-worker' is
  // anyway not honored by speculation rules prefetch.
  Common.Settings.settingForTest('bypass-service-worker').set(false);
  await forceSpeculationRules(scope);

  // Navigate to the URL (served by the network), i.e. the prefetched result
  // shouldn't be used.
  Common.Settings.settingForTest('bypass-service-worker').set(true);
  await testNavigation(scope);

  TestRunner.completeTest();
})();
