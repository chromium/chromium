// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startWithFrameControl(
      'Tests screenshot produced by beginFrame honors optimizeForSpeed for ' +
      'WEBP encoding');

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://example.com/`, `<!doctype html>
      <html><body>
        <style>
          div {
            width: 400px;
            height: 300px;
            background: repeating-linear-gradient(
                45deg, #f00, #00f 10px, #0f0 20px);
          }
        </style>
        <div></div>
        <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>
      </body></html>
  `);

  await virtualTimeController.initialize(100);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(500);

  let frameTimeOffset = 0;
  async function takeScreenshot(quality, optimizeForSpeed) {
    return (await dp.HeadlessExperimental.beginFrame({
             frameTimeTicks:
                 virtualTimeController.currentFrameTime() + frameTimeOffset++,
             screenshot: {format: 'webp', quality, optimizeForSpeed}
           }))
        .result.screenshotData;
  }

  async function compare(kind, quality) {
    const slow = await takeScreenshot(quality, false);
    const fast = await takeScreenshot(quality, true);
    if (slow.length < fast.length) {
      testRunner.log(
          'PASSED: slowly encoded ' + kind +
          ' webp is smaller than quickly encoded');
    } else {
      testRunner.log(`FAILED: ${slow.length} >= ${fast.length}`);
    }
  }

  await compare('lossless', 100);
  await compare('lossy', 80);
  testRunner.completeTest();
})
