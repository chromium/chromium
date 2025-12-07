// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that `Page.frameSubtreeWillBeDetached` event is emitted for each iFrame before the frames are detached');

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

  for (const url of testUrls) {
    await frameSubtreeWillBeDetachedHelper.createAndWaitIframe(testRunner, session,
        navigationHelper, url);
  }

  testRunner.log('... removing all the iframes');
  await session.evaluate('document.body.innerHTML = ""');
  testRunner.log('... done removing all the iframes');

  testRunner.completeTest();
});