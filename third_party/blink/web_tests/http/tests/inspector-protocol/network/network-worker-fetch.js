// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests fetch in worker.\n');

  const targetUrl =
      testRunner.url('resources/get-response-body-valid-text.php');
  const uuidBlobRegex =
      /^blob:http:\/\/127\.0\.0\.1:8000\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;

  const requests = [];
  const requestsById = new Map();
  let completedCount = 0;

  let resolveAllFinished;
  const allFinished = new Promise(resolve => resolveAllFinished = resolve);

  function markDone(entry) {
    if (!entry || entry.done) {
      return;
    }
    entry.done = true;
    ++completedCount;
    if (completedCount === 2) {
      resolveAllFinished();
    }
  }

  function trackNetwork(protocol) {
    protocol.Network.onRequestWillBeSent(({params}) => {
      if (requestsById.has(params.requestId)) {
        return;
      }
      const entry = {
        url: params.request.url,
        type: (params.type || 'Other').toLowerCase(),
        status: null,
        failed: false,
        done: false,
      };
      requests.push(entry);
      requestsById.set(params.requestId, entry);
    });
    protocol.Network.onResponseReceived(({params}) => {
      const entry = requestsById.get(params.requestId);
      if (entry) {
        if (params.type) {
          entry.type = params.type.toLowerCase();
        }
        entry.status = params.response.status;
      }
    });
    protocol.Network.onLoadingFinished(({params}) => {
      markDone(requestsById.get(params.requestId));
    });
    protocol.Network.onLoadingFailed(({params}) => {
      const entry = requestsById.get(params.requestId);
      if (entry) {
        entry.failed = true;
        markDone(entry);
      }
    });
  }

  trackNetwork(dp);
  await dp.Network.enable();

  dp.Target.onAttachedToTarget(async ({params}) => {
    const wdp = session.createChild(params.sessionId).protocol;
    trackNetwork(wdp);
    await Promise.all([
      wdp.Network.enable(),
      wdp.Runtime.runIfWaitingForDebugger(),
    ]);
  });

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const workerScript = `
    self.onmessage = (event) => {
      fetch(new Request(event.data.url, event.data.init)).then(
          (response) => response.text().then((text) => self.postMessage(text)),
          () => self.postMessage('FETCH_FAILED'));
    };
  `;

  const result = await session.evaluateAsync((script, url) => {
    return new Promise((resolve) => {
      const blob = new Blob([script], {type: 'application/javascript'});
      const workerUrl = URL.createObjectURL(blob);
      const worker = new Worker(workerUrl);
      worker.onmessage = (event) => {
        URL.revokeObjectURL(workerUrl);
        resolve(event.data);
      };
      worker.postMessage({url, init: {}});
    });
  }, workerScript, targetUrl);

  await allFinished;

  if (requests.length !== 2 ||
      requests.some(request => request.status !== 200)) {
    testRunner.fail('Unexpected worker fetch network state.');
    return;
  }

  testRunner.log('Fetch in worker result: ' + result);
  for (const request of requests) {
    const displayUrl = uuidBlobRegex.test(request.url) ?
        'blob:http://127.0.0.1:8000/<UUID>' :
        request.url;
    testRunner.log(displayUrl);
    testRunner.log('resource.type: ' + request.type);
    testRunner.log('request.failed: ' + request.failed);
  }
  testRunner.completeTest();
});
