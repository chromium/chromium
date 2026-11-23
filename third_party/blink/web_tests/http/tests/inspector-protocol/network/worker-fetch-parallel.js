// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that parallel fetches in worker should not cause crash.');

  const requests = new Map();
  let resolveAllDone;
  const allDone = new Promise(resolve => {
    resolveAllDone = resolve;
  });

  function maybeComplete() {
    if (requests.size === 3 &&
        Array.from(requests.values())
            .every(req =>
                       (req.responseReceived || req.failed) && req.finished)) {
      resolveAllDone();
    }
  }

  function trackNetworkEvents(protocol) {
    protocol.Network.onRequestWillBeSent(event => {
      const {requestId, request, type} = event.params;
      if (!requests.has(requestId)) {
        requests.set(requestId, {
          url: request.url,
          type,
          failed: false,
          responseReceived: false,
          finished: false,
        });
      }
    });
    protocol.Network.onResponseReceived(event => {
      const entry = requests.get(event.params.requestId);
      if (entry) {
        entry.type = event.params.type;
        entry.responseReceived = true;
        maybeComplete();
      }
    });
    protocol.Network.onLoadingFinished(event => {
      const entry = requests.get(event.params.requestId);
      if (entry) {
        entry.finished = true;
        maybeComplete();
      }
    });
    protocol.Network.onLoadingFailed(event => {
      const entry = requests.get(event.params.requestId);
      if (entry) {
        entry.failed = true;
        entry.finished = true;
        maybeComplete();
      }
    });
  }

  await dp.Network.enable();
  trackNetworkEvents(dp);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    const workerDp = session.createChild(event.params.sessionId).protocol;
    trackNetworkEvents(workerDp);
    await workerDp.Network.enable();
    await workerDp.Runtime.runIfWaitingForDebugger();
  });

  const workerResult = await session.evaluateAsync(`
    new Promise((resolve, reject) => {
      const worker = new Worker('/inspector-protocol/network/resources/fetch-parallel-worker.js');
      worker.onmessage = (event) => {
        resolve(JSON.stringify(event.data));
      };
      worker.onerror = (event) => {
        reject(new Error(event.message || 'Worker error'));
      };
      worker.postMessage([
        '/inspector-protocol/network/resources/hello-world.html?1',
        '/inspector-protocol/network/resources/hello-world.html?2',
      ]);
    })
  `);

  await allDone;

  testRunner.log('Parallel fetch in worker result: ' + workerResult);
  const sortedRequests =
      Array.from(requests.values()).sort((a, b) => a.url.localeCompare(b.url));
  for (const request of sortedRequests) {
    testRunner.log(request.url);
    testRunner.log('resource.type: ' + request.type);
    testRunner.log('request.failed: ' + request.failed);
  }
  testRunner.completeTest();
});
