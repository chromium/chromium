// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as ProtocolClient from 'devtools/core/protocol_client/protocol_client.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Test that if a profiler is working all the agents are disabled.\n`);

  var messages = [];
  function collectMessages(message) {
    messages.push(message);
  }
  ProtocolClient.InspectorBackend.test.dumpProtocol = collectMessages;
  messages.push('--> SDK.TargetManager.TargetManager.instance().suspendAllTargets();');
  await SDK.TargetManager.TargetManager.instance().suspendAllTargets();
  messages.push('');
  messages.push('--> SDK.TargetManager.TargetManager.instance().resumeAllTargets();');
  await SDK.TargetManager.TargetManager.instance().resumeAllTargets();
  messages.push('');
  messages.push('--> done');
  ProtocolClient.InspectorBackend.test.dumpProtocol = null;
  for (var i = 0; i < messages.length; ++i) {
    var message = messages[i];
    if (message.startsWith('backend')) {
      continue;
    }
    message = message.replace(/"id":\d+,/, '"id":<number>,').replace(/"sessionId":"[0-9A-F]+"/, '"sessionId":<string>');
    TestRunner.addResult(message);
  }
  TestRunner.completeTest();
})();
