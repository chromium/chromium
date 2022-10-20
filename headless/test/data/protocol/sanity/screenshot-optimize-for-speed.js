// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startWithFrameControl(
      'Tests screenshot produced by beginFrame honors optimizeForSpeed param');

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://example.com/`,
      `<!doctype html>
      <html><body>Hello world</body></html>
  `);

  await virtualTimeController.initialize(100);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(500);

  let frameTimeOffset = 0;
  async function takeScreenshot(optimizeForSpeed) {
    return (await dp.HeadlessExperimental.beginFrame({
        frameTimeTicks: virtualTimeController.currentFrameTime() +
            frameTimeOffset++,
        screenshot: {
          format: 'png',
          optimizeForSpeed
        }
    })).result.screenshotData;
  }

  const slow = await takeScreenshot(false);
  const fast = await takeScreenshot(true);

  testRunner.log(slow.length < fast.length
                     ? 'PASSED: slowly-encoded PNG is smaller that fast-encoded'
                     : `FAILED: ${slow.length} >= ${fast.length}}`);
  testRunner.completeTest();
})