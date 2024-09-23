// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that `Page.frameSubtreeWillBeDetached` event is emitted before the frame subtree is detached');

  const testUrls = [
    // Page -> same origin iframe -> same origin iframe.
    'http://127.0.0.1:8000/inspector-protocol/resources/same_origin_iframe.html',
    // Page -> same origin iframe -> cross origin iframe.
    'http://127.0.0.1:8000/inspector-protocol/resources/cross_origin_iframe.html',
    // Page -> cross origin iframe -> same origin iframe.
    'http://oopif.test:8000/inspector-protocol/resources/cross_origin_iframe.html',
    // Page -> cross origin iframe -> cross origin iframe.
    'http://devtools.test:8000/inspector-protocol/resources/cross_origin_iframe.html',
  ];

  const frameSubtreeWillBeDetachedHelper = await testRunner.loadScript(
      './resources/frame-subtree-will-be-detached-helper.js');
  const navigationHelper = await testRunner.loadScript(
      '../resources/navigation-helper.js');

  await frameSubtreeWillBeDetachedHelper.initTest(testRunner, dp, session,
      navigationHelper);

  async function navigateAndRemoveIframe(url) {
    await frameSubtreeWillBeDetachedHelper.createAndWaitIframe(testRunner, session,
        navigationHelper, url);

    testRunner.log('... removing the frame');
    await session.evaluate('frame.remove()');
    testRunner.log(`... done removing the frame`);
  }

  for (const url of testUrls) {
    await navigateAndRemoveIframe(url);
  }

  testRunner.completeTest();
});