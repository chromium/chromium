// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Verifies that the main page request repeated by a service worker appears in the network log.');

  const origin = 'http://127.0.0.1:8000';
  await dp.Storage.clearDataForOrigin({origin, storageTypes: 'all'});

  await dp.Page.enable();
  await dp.Target.setAutoAttach({
    autoAttach: true,
    waitForDebuggerOnStart: true,
    flatten: true,
  });

  const pageTargetType =
      (await dp.Target.getTargetInfo()).result.targetInfo.type;

  const requests = [];
  let recording = false;
  let resolveBothSeen;
  const bothSeenPromise = new Promise(resolve => {
    resolveBothSeen = resolve;
  });

  const recordRequest = (targetType, event) => {
    if (!recording) {
      return;
    }
    requests.push({
      url: event.params.request.url,
      targetType,
    });
    if (requests.some(r => r.targetType === pageTargetType) &&
        requests.some(r => r.targetType === 'service_worker')) {
      resolveBothSeen();
    }
  };

  let resolveSwAttached;
  const swAttachedPromise = new Promise(resolve => {
    resolveSwAttached = resolve;
  });

  dp.Target.onAttachedToTarget(async attachedEvent => {
    const targetType = attachedEvent.params.targetInfo.type;
    if (targetType !== 'service_worker') {
      return;
    }
    const swDp = session.createChild(attachedEvent.params.sessionId).protocol;
    swDp.Network.onRequestWillBeSent(
        requestEvent => recordRequest(targetType, requestEvent));
    await Promise.all([
      swDp.Network.enable(),
      swDp.Runtime.runIfWaitingForDebugger(),
    ]);
    resolveSwAttached();
  });

  await session.navigate('resources/service-worker-repeat-fetch.html');
  await Promise.all([
    session.evaluateAsync('installSWAndWaitForActivated()'),
    swAttachedPromise,
  ]);

  await dp.Network.enable();
  dp.Network.onRequestWillBeSent(
      requestEvent => recordRequest(pageTargetType, requestEvent));

  recording = true;
  await Promise.all([
    dp.Page.reload(),
    dp.Page.onceLoadEventFired(),
    bothSeenPromise,
  ]);
  recording = false;

  testRunner.log('Page reloaded.');
  testRunner.log('');
  for (const request of requests) {
    testRunner.log(`request.url(): ${request.url}`);
    testRunner.log(`request.target.type(): ${request.targetType}`);
    testRunner.log('');
  }

  await dp.Storage.clearDataForOrigin({origin, storageTypes: 'all'});
  testRunner.completeTest();
})
