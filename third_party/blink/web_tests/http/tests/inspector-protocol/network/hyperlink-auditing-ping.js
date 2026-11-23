// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pingUrl = testRunner.url('./resources/204.pl');
  const {session, dp} = await testRunner.startHTML(
      `<a id="pingLink" href="#done" ping="${pingUrl}">ping</a>`,
      'Tests that hyperlink auditing (ping) requests are reported.');

  await dp.Network.enable();

  await session.evaluate(`
      testRunner.overridePreference("WebKitHyperlinkAuditingEnabled", 1);
      function navigateLink() {
        const evt = document.createEvent("MouseEvents");
        evt.initMouseEvent("click");
        const link = document.getElementById("pingLink");
        link.dispatchEvent(evt);
      }
  `);

  const finishedRequests = new Set();
  let targetRequestId;
  let resolveLoadingFinished;
  const loadingFinishedPromise = new Promise(resolve => {
    resolveLoadingFinished = resolve;
  });
  const onLoadingFinished = event => {
    finishedRequests.add(event.params.requestId);
    if (targetRequestId && event.params.requestId === targetRequestId) {
      resolveLoadingFinished();
    }
  };
  dp.Network.onLoadingFinished(onLoadingFinished);

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  await session.evaluate('navigateLink()');
  const {params} = await requestWillBeSentPromise;
  targetRequestId = params.requestId;
  if (!finishedRequests.has(targetRequestId)) {
    await loadingFinishedPromise;
  }
  dp.Network.offLoadingFinished(onLoadingFinished);

  const headers = params.request.headers;
  const contentType = headers['Content-Type'] ?? headers['content-type'];
  testRunner.log(`url: ${params.request.url}`);
  testRunner.log(`method: ${params.request.method}`);
  testRunner.log(`type: ${params.type}`);
  testRunner.log(`Content-Type: ${contentType}`);
  testRunner.completeTest();
});
