// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      '../resources/quirks-mode.html',
      'Tests that content is correctly shown for css loaded with invalid mime type in quirks mode. https://bugs.webkit.org/show_bug.cgi?id=80528\n');

  const compatMode = await session.evaluate('document.compatMode');
  testRunner.log(`document.compatMode: ${compatMode}`);

  await dp.Page.enable();
  await dp.Network.enable();
  await dp.DOM.enable();
  await dp.CSS.enable();

  const stylesheetUrl = testRunner.url('resources/stylesheet-text-plain.php');
  const finishedRequestIds = new Set();
  const finishedCallbacks = new Map();
  const failedRequests = new Map();

  dp.Network.onLoadingFinished(event => {
    const {requestId} = event.params;
    finishedRequestIds.add(requestId);
    const cb = finishedCallbacks.get(requestId);
    if (cb) {
      finishedCallbacks.delete(requestId);
      cb();
    }
  });
  dp.Network.onLoadingFailed(event => {
    failedRequests.set(event.params.requestId, event.params.errorText);
  });

  function waitForLoadingFinished(requestId) {
    if (finishedRequestIds.has(requestId)) {
      return Promise.resolve();
    }
    return new Promise(resolve => finishedCallbacks.set(requestId, resolve));
  }

  const responseReceivedPromise = dp.Network.onceResponseReceived(
      event => event.params.response.url === stylesheetUrl);
  const styleSheetAddedPromise = dp.CSS.onceStyleSheetAdded(
      event => event.params.header.sourceURL === stylesheetUrl);

  await session.evaluateAsync(url => {
    return new Promise((resolve, reject) => {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = url;
      link.onload = () => {
        window.getComputedStyle(document.body).color;
        resolve();
      };
      link.onerror = () => reject(new Error('Stylesheet failed to load'));
      document.head.append(link);
    });
  }, stylesheetUrl);

  const {params: responseParams} = await responseReceivedPromise;
  const stylesheetRequestId = responseParams.requestId;
  await waitForLoadingFinished(stylesheetRequestId);
  if (failedRequests.has(stylesheetRequestId)) {
    testRunner.log(`FAIL: Network.loadingFailed: ${
        failedRequests.get(stylesheetRequestId)}`);
  }

  const {params: {header}} = await styleSheetAddedPromise;

  const {result: {frameTree}} = await dp.Page.getResourceTree();
  const pageResource =
      frameTree.resources.find(resource => resource.url === stylesheetUrl);
  if (!pageResource) {
    testRunner.log(
        'FAIL: stylesheet resource missing from Page.getResourceTree');
    testRunner.completeTest();
    return;
  }

  testRunner.log(pageResource.url);
  testRunner.log(`Page resource type: ${pageResource.type}`);
  testRunner.log(`Page resource mimeType: ${pageResource.mimeType}`);
  testRunner.log(`Page resource failed: ${Boolean(pageResource.failed)}`);
  testRunner.log(`Network response type: ${responseParams.type}`);
  testRunner.log(
      `Network response mimeType: ${responseParams.response.mimeType}`);
  testRunner.log(`Network response status: ${responseParams.response.status}`);
  testRunner.log(`CSS header loadingFailed: ${header.loadingFailed}`);

  const {result: pageContentResult} = await dp.Page.getResourceContent({
    frameId: frameTree.frame.id,
    url: stylesheetUrl,
  });
  const {result: cssTextResult} = await dp.CSS.getStyleSheetText({
    styleSheetId: header.styleSheetId,
  });
  const {result: networkBodyResult} = await dp.Network.getResponseBody({
    requestId: stylesheetRequestId,
  });

  const pageContent = pageContentResult.base64Encoded ?
      atob(pageContentResult.content) :
      pageContentResult.content;
  const networkContent = networkBodyResult.base64Encoded ?
      atob(networkBodyResult.body) :
      networkBodyResult.body;

  if (cssTextResult.text !== pageContent || networkContent !== pageContent) {
    testRunner.log('FAIL: content mismatch across Page/CSS/Network');
    testRunner.log(`Page.getResourceContent: ${JSON.stringify(pageContent)}`);
    testRunner.log(
        `CSS.getStyleSheetText: ${JSON.stringify(cssTextResult.text)}`);
    testRunner.log(
        `Network.getResponseBody: ${JSON.stringify(networkContent)}`);
  }

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId: bodyNodeId}} = await dp.DOM.querySelector({
    nodeId: root.nodeId,
    selector: 'body',
  });
  const {result: {matchedCSSRules}} = await dp.CSS.getMatchedStylesForNode({
    nodeId: bodyNodeId,
  });
  const matchedBodyRules =
      (matchedCSSRules ||
       []).filter(match => match.rule.styleSheetId === header.styleSheetId);
  testRunner.log(
      `Matched stylesheet rules on body: ${matchedBodyRules.length}`);

  testRunner.log('Resource.content: ' + pageContent);
  testRunner.completeTest();
});
