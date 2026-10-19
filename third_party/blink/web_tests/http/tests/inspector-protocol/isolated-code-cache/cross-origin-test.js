// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests V8 code cache for javascript resources\n`);
  const CodeCacheHelper =
      await testRunner.loadScript('resources/code-cache-helper.js');
  const helper = new CodeCacheHelper(testRunner, session,
                                     ['v8.compile', 'v8.produceCache']);

  // Clear browser cache to avoid any existing entries for the fetched
  // scripts in the cache.
  await dp.Network.clearBrowserCache();

  await session.navigate(
      'http://127.0.0.1:8000/devtools/resources/test-page-v8-code-cache.html');

  const scope =
      'http://127.0.0.1:8000/inspector-protocol/service-worker/resources/v8-cache-iframe.html';
  const frameId = 'frame_id';

  await helper.addIframe(scope, frameId);

  testRunner.log(
      '---First navigation - produce and consume code cache ------\n');

  // This loads the same script thrice. With the current V8 heuristics (defined
  // in third_party/blink/renderer/bindings/core/v8/v8_code_cache.cc) we produce
  // cache on second fetch and consume it in the third fetch. We may have to
  // change this if the heuristics change.
  await helper.startTracing();
  await session.evaluateAsync(async frameId => {
    function waitUntilIdle() {
      return new Promise(resolve => window.requestIdleCallback(resolve));
    }
    const url = 'http://localhost:8000/devtools/resources/v8-cache-script.js';
    const iframeWindow = document.getElementById(frameId).contentWindow;
    await iframeWindow.loadScript(url);
    await iframeWindow.loadScript(url);
    await waitUntilIdle();
    await iframeWindow.loadScript(url);
  }, frameId);
  await helper.stopTracingAndPrint();

  // Second navigation
  testRunner.log('\n--- Second navigation - from a different origin ------\n');

  await session.navigate(
      'http://localhost:8000/devtools/resources/test-page-v8-code-cache.html');

  const localhostScope =
      'http://localhost:8000/inspector-protocol/service-worker/resources/v8-cache-iframe.html';

  await helper.addIframe(localhostScope, frameId);
  await helper.startTracing();
  await session.evaluateAsync(frameId => {
    const url = 'http://localhost:8000/devtools/resources/v8-cache-script.js';
    const iframeWindow = document.getElementById(frameId).contentWindow;
    return iframeWindow.loadScript(url);
  }, frameId);
  await helper.stopTracingAndPrint();

  testRunner.completeTest();
})
