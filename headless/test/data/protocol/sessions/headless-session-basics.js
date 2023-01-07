// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  testRunner.log('Tests headless session basics.\n');

  // HeadlessDevToolsSession handles Target.createTarget. So, it's responsible
  // for adding the sessionId into its return value. This test uses the
  // low-level browser protocol connection, attached to a browser target,
  // and creates a browser context. In the subsequent Target.createTarget
  // call, we observe that the return value carries the expected sessionId.

  testRunner.browserP().Target.attachToBrowserTarget();
  const sessionId =
        (await testRunner.browserP().Target.onceAttachedToTarget())
        .params.sessionId;

  const session = new TestRunner.Session(testRunner, sessionId);

  const browserContextId =
        (await session.protocol.Target.createBrowserContext()).browserContextId;

  const returnValue = (await session.protocol.Target.createTarget({
    'browserContextId': browserContextId,
    'enableBeginFrameControl': true, 'height': 1, 'url': 'about:blank',
    'width': 1}));

  // If HeadlessDevToolsSession fails to put the sessionId into the response,
  // this log line will be missing.
  if (returnValue.sessionId && returnValue.sessionId.length > 0) {
    testRunner.log('SUCCESS: Target.createTarget returned sessionId.\n');
  }

  testRunner.completeTest();
})
