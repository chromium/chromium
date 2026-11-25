// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that if an iframe is loaded and then deleted, inspector can still return its response body, including when iframe.src is changed to a javascript: URL after loading.`);

  await dp.Network.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Page.enable();
  await dp.Runtime.enable();

  const iframeUrl = testRunner.url('resources/hello-world.html');
  const requests = [];
  const requestsById = new Map();
  const detachedFrames = new Set();
  let resolveBothDetached;
  const bothDetached = new Promise(resolve => {
    resolveBothDetached = resolve;
  });

  function maybeResolveDetached() {
    if (requests.length === 2 &&
        requests.every(req => detachedFrames.has(req.frameId))) {
      resolveBothDetached();
    }
  }

  dp.Page.onFrameDetached(event => {
    detachedFrames.add(event.params.frameId);
    maybeResolveDetached();
  });

  dp.Network.onRequestWillBeSent(event => {
    const {requestId, request, frameId} = event.params;
    if (request.url !== iframeUrl) {
      testRunner.fail(`Unexpected request URL: ${request.url}`);
      return;
    }
    let resolveFinished;
    const finished = new Promise(resolve => {
      resolveFinished = resolve;
    });
    const entry = {
      requestId,
      url: request.url,
      frameId,
      type: null,
      status: null,
      finished,
      resolveFinished,
    };
    requests.push(entry);
    requestsById.set(requestId, entry);
    maybeResolveDetached();
  });

  dp.Network.onResponseReceived(event => {
    const entry = requestsById.get(event.params.requestId);
    if (!entry) {
      return;
    }
    entry.type = event.params.type;
    entry.status = event.params.response.status;
  });

  dp.Network.onLoadingFinished(event => {
    const entry = requestsById.get(event.params.requestId);
    if (entry) {
      entry.resolveFinished();
    }
  });

  dp.Network.onLoadingFailed(event => {
    testRunner.fail(`Request failed (${event.params.requestId}): ${
        event.params.errorText}`);
  });

  const doneConsoleMessage = dp.Runtime.onceConsoleAPICalled(
      event => event.params.type === 'log' &&
          event.params.args[0]?.value === 'Done.');

  await session.evaluate(url => {
    let iframe;

    function loadIframe() {
      iframe = document.createElement('iframe');
      document.body.appendChild(iframe);
      iframe.onload = iframeLoaded;
      iframe.src = url;
    }

    function iframeLoaded() {
      document.body.removeChild(iframe);
      loadIframeAndChangeSrcAfterLoad();
    }

    function loadIframeAndChangeSrcAfterLoad() {
      iframe = document.createElement('iframe');
      document.body.appendChild(iframe);
      iframe.onload = iframeLoadedChangeSrc;
      iframe.src = url;
    }

    function iframeLoadedChangeSrc() {
      iframe.onload = null;
      iframe.src = 'javascript:\'<html></html>\'';
      document.body.removeChild(iframe);
      console.log('Done.');
    }

    loadIframe();
  }, iframeUrl);

  await doneConsoleMessage;
  if (requests.length !== 2) {
    testRunner.fail(`Expected 2 requests, got ${requests.length}`);
  }
  await Promise.all(requests.map(req => req.finished));
  await bothDetached;

  for (const [index, req] of requests.entries()) {
    const response =
        await dp.Network.getResponseBody({requestId: req.requestId});
    if (response.error) {
      testRunner.fail(`Case ${index + 1} getResponseBody error: ${
          JSON.stringify(response.error)}`);
      continue;
    }
    testRunner.log(`Case ${index + 1} URL: ${req.url}`);
    testRunner.log(`resource.type: ${req.type}`);
    testRunner.log(`response.status: ${req.status}`);
    testRunner.log(`base64Encoded: ${response.result.base64Encoded}`);
    testRunner.log(
        `resource.content after requesting content:\n${response.result.body}`);
  }

  await dp.Network.setCacheDisabled({cacheDisabled: false});
  testRunner.completeTest();
})
