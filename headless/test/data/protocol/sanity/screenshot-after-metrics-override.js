// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests that screenshot right after viewport resize doesn\'t hang');

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.HeadlessExperimental.enable();

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://green.com/`,
      `<style>
         body { background-color: green; }
       </style>
       <body></body>`
  );

  await virtualTimeController.initialize(100);
  await frameNavigationHelper.navigate('http://green.com/');
  await virtualTimeController.grantTime(500);

  await dp.Emulation.setDeviceMetricsOverride({
      deviceScaleFactor: 1,
      width: 1024, height: 1024,
      mobile: false,
      screenWidth: 1024, screenHeight: 1024,
      viewport: { x: 0, y: 0, width: 1024, height: 1024, scale: 1 }
  });
  const ctx = await virtualTimeController.captureScreenshot();
  if (ctx) {
    let rgba = ctx.getImageData(25, 25, 1, 1).data;
    testRunner.log(`rgba @(25,25) : ${rgba}`);
  } else {
    testRunner.log('FAIL: screenshot data missing!');
  }
  testRunner.completeTest();
})
