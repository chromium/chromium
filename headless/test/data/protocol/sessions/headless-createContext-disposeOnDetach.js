// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  testRunner.log('Tests headless context destruction on session close.\n');

  async function createSession() {
    const {result: {sessionId}} =
        await testRunner.browserP().Target.attachToBrowserTarget();
    return new TestRunner.Session(testRunner, sessionId);
  }

  async function dumpContextNumber() {
    const session = await createSession();
    const {result} = await session.protocol.Target.getBrowserContexts();
    testRunner.log(
        'Number of contexts created: ' + result.browserContextIds.length);
  }

  const session1 = await createSession();
  for (let i = 0; i < 3; ++i) {
    testRunner.log('Creating context {disposeOnDetach: true} in session1');
    await session1.protocol.Target.createBrowserContext(
        {disposeOnDetach: true});
  }
  await dumpContextNumber();

  const session2 = await createSession();
  for (let i = 0; i < 2; ++i) {
    testRunner.log('Creating context {disposeOnDetach: false} in session2');
    await session2.protocol.Target.createBrowserContext(
        {disposeOnDetach: false});
  }
  await dumpContextNumber();

  testRunner.log('Detaching from session1');
  await session1.disconnect();
  await dumpContextNumber();

  testRunner.log('Detaching from session2');
  await session2.disconnect();
  await dumpContextNumber();

  testRunner.completeTest();
})
