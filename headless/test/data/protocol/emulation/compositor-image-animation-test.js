// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests compositor animated image handling.');

  // Loads an animated GIF into a a newly created tab with BeginFrame control
  // and verifies that:
  // - animate_only BeginFrames don't produce CompositorFrames,
  // - first screenshot starts the GIF animation,
  // - animation is advanced according to virtual time.
  // - the animation is not resynced after the first iteration.

  const VirtualTimeController =
      await testRunner.loadScript('../helpers/virtual-time-controller.js');
  const virtual_time_controller =
      new VirtualTimeController(testRunner, dp, 100);
  await virtual_time_controller.initialize(1000);

  dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  await dp.Page.navigate(
    {url: testRunner.url('/resources/compositor-image-animation.html')});

  // Color must be blue -- we start with blue, and this is entire cycle after.
  await virtual_time_controller.grantTime(3500);
  await dumpPixelColor();
  // Color must be yellow here.
  await virtual_time_controller.grantTime(2000);
  await dumpPixelColor();
  // Full cycle later, we still expect yellow.
  await virtual_time_controller.grantTime(3000);
  await dumpPixelColor();

  testRunner.completeTest();

  async function dumpPixelColor() {
    testRunner.log(`Current time: ${virtual_time_controller.elapsedTime()}`);
    const ctx = await virtual_time_controller.captureScreenshot();
    const rgba = ctx.getImageData(0, 0, 1, 1).data;
    testRunner.log(`Screenshot rgba: ${rgba}`);
  }
})
