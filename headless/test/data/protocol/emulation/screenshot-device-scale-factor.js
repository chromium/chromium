// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests screenshot with overridden device scale factor');

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  dp.Emulation.enable();
  await dp.Emulation.setDeviceMetricsOverride({
      deviceScaleFactor: 2,
      width: 100,
      height: 100,
      screenHeight: 100,
      screenWidth: 100,
      mobile: true,
      viewport: {x: 0, y: 0, width: 100, height: 100, scale: 1}
  });

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://example.com/`,
      `<!doctype html>
      <html>
      <meta name=viewport content="width=device-width">
      <style>
        html, body { width: 100%; height: 100%; margin:0; padding:0; }
        div {
          width: 100%; height: 100%; margin:0; padding:0;
          background-size: 100% 100%;
          background-color: blue;
        }
      </style>
      <body>
        <div></div>
      </body>
      </html>
  `);

  await virtualTimeController.initialize(100);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(500);
  const ctx = await virtualTimeController.captureScreenshot();
  // We use a screen and viewport of 100x100 DIP, which is 200 physical pixels
  // due to deviceScaleFactor of 2. Make sure the screenshot is not clipped.
  let rgba = ctx.getImageData(175, 175, 1, 1).data;
  testRunner.log(`rgba @(175,175) ${rgba}`);
  testRunner.completeTest();
})