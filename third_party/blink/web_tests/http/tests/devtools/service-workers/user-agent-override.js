// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that User-Agent override works for requests from Service Workers.\n`);
  await TestRunner.loadLegacyModule('console');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('resources');

  function waitForTarget() {
    return new Promise(function(resolve) {
      var sniffer = {
        targetAdded: function(target) {
          if (target.type() === SDK.Target.Type.ServiceWorker) {
            resolve();
            SDK.targetManager.unobserveTargets(sniffer);
          }
        },

        targetRemoved: function(e) {}
      };
      SDK.targetManager.observeTargets(sniffer);
    });
  }

  function waitForConsoleMessage(regex) {
    return new Promise(function(resolve) {
      SDK.targetManager.addModelListener(SDK.ConsoleModel, SDK.ConsoleModel.Events.MessageAdded, sniff);

      function sniff(e) {
        if (e.data && regex.test(e.data.messageText)) {
          resolve(e.data);
          SDK.consoleModel.removeEventListener(SDK.ConsoleModel.Events.MessageAdded, sniff);
        }
      }
    });
  }

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/user-agent-override-worker.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/user-agent-override/';
  var userAgentString = 'Mozilla/5.0 (Overridden User Agent)';
  var originalUserAgent = navigator.userAgent;

  TestRunner.addResult('Enable emulation and set User-Agent override');
  SDK.multitargetNetworkManager.setUserAgentOverride(userAgentString);

  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await waitForTarget();
  await ApplicationTestRunner.postToServiceWorker(scope, 'message');
  let msg = await waitForConsoleMessage(/HTTP_USER_AGENT/);

  TestRunner.addResult('Overriden user agent: ' + msg.messageText);
  TestRunner.addResult('Disable emulation');
  SDK.multitargetNetworkManager.setUserAgentOverride('');

  await ApplicationTestRunner.unregisterServiceWorker(scope);
  await ApplicationTestRunner.registerServiceWorker(scriptURL + '?2', scope);
  await waitForTarget();
  await ApplicationTestRunner.postToServiceWorker(scope, 'message');
  msg = await waitForConsoleMessage(/HTTP_USER_AGENT/);

  TestRunner.addResult('User agent without override is correct: ' + (msg.messageText != userAgentString));
  ApplicationTestRunner.unregisterServiceWorker(scope);

  TestRunner.addResult('Test complete');
  TestRunner.completeTest();
})();
