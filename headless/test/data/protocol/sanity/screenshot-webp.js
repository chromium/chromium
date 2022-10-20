// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startWithFrameControl(
      'Tests screenshot produced by beginFrame supports WEBP encoding');

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://example.com/`,
      `<!doctype html>
      <html><body></body></html>
  `);

  await virtualTimeController.initialize(100);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(500);

  const screenshotData = (await dp.HeadlessExperimental.beginFrame({
      frameTimeTicks: virtualTimeController.currentFrameTime(),
      screenshot: { format: 'webp' }
  })).result.screenshotData;

  const buffer = await fetch(`data:image/webp;base64,${screenshotData}`)
      .then(r => r.arrayBuffer());
  const contents = new Int8Array(buffer);
  function SliceAsString(arr, from, to) {
    return String.fromCharCode.apply(null, arr.slice(from, to));
  }
  testRunner.log(`RIFF signature: ${SliceAsString(contents, 0, 4)}`);
  testRunner.log(`WEBP signature: ${SliceAsString(contents, 8, 12)}`);
  testRunner.completeTest();
})