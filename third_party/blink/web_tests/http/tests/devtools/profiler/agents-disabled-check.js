// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that if a profiler is working all the agents are disabled.\n`);

  var messages = [];
  function collectMessages(message) {
    messages.push(message);
  }
  ProtocolClient.test.dumpProtocol = collectMessages;
  messages.push('--> SDK.targetManager.suspendAllTargets();');
  await SDK.targetManager.suspendAllTargets();
  messages.push('');
  messages.push('--> SDK.targetManager.resumeAllTargets();');
  await SDK.targetManager.resumeAllTargets();
  messages.push('');
  messages.push('--> done');
  ProtocolClient.test.dumpProtocol = null;
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
