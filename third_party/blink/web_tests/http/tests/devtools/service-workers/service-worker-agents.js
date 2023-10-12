// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests the way service workers don't enable DOM agent and does enable Debugger agent.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/';

  TestRunner.addSniffer(SDK.Connections.MainConnection.prototype, 'sendRawMessage', function(messageString) {
    var message = JSON.parse(messageString);
    if (!message.sessionId || message.sessionId === SDK.TargetManager.TargetManager.instance().primaryPageTarget().sessionId)
      return;
    if (messageString.includes('DOM.'))
      TestRunner.addResult('DOM-related command should NOT be issued: ' + messageString);
    if (messageString.includes('CSS.'))
      TestRunner.addResult('CSS-related command should NOT be issued: ' + messageString);
    if (messageString.includes('Debugger.enable')) {
      TestRunner.addResult(
          'Debugger-related command should be issued: ' + JSON.stringify(inlineMessages(message), null, 4));
    }
  }, true);

  ApplicationTestRunner.waitForServiceWorker(step1);
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);

  async function step1(target) {
    TestRunner.addResult('Suspending targets.');
    await SDK.TargetManager.TargetManager.instance().suspendAllTargets();
    TestRunner.addResult('Resuming targets.');
    await SDK.TargetManager.TargetManager.instance().resumeAllTargets();
    TestRunner.completeTest();
  }

  function inlineMessages(obj) {
    for (var key in obj) {
      if (key === 'message' && typeof obj[key] === 'string') {
        obj[key] = JSON.parse(obj[key]);
        inlineMessages(obj[key]);
      } else if (key === 'id' || key.endsWith('Id'))
        obj[key] = '<id>';
      if (typeof obj[key] === 'object')
        inlineMessages(obj[key]);
    }
    return obj;
  }
})();
