// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that memory-cached resources are correctly reported.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.navigatePromise(`resources/memory-cached-resource.html`);

  function waitOnResource(url, status, cached) {
    return new Promise(resolve => {
      const eventName = SDK.NetworkManager.Events.RequestFinished;
      function onRequestFinished(event) {
        const request = event.data;
        if (url.test(request.url()) && status === request.statusCode && cached === request.cached()) {
          TestRunner.networkManager.removeEventListener(eventName, onRequestFinished);
          resolve(request);
        }
      }
      TestRunner.networkManager.addEventListener(eventName, onRequestFinished);
    });
  }

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.reloadPage();
  await waitOnResource(/abe\.png/, 200, false);
  TestRunner.addResult('An uncached resource is found.');

  await TestRunner.NetworkAgent.setCacheDisabled(false);
  const cached = waitOnResource(/abe\.png/, 200, true);
  await TestRunner.addIframe('memory-cached-resource.html');
  await cached;
  TestRunner.addResult('A cached resource is found.');

  TestRunner.completeTest();
})();
