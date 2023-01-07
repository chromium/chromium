// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests compositor animated image handling.');

  // Loads an animated GIF into a a newly created tab with BeginFrame control
  // and verifies that:
  // - animate_only BeginFrames don't produce CompositorFrames,
  // - first screenshot starts the GIF animation,
  // - animation is advanced according to virtual time.
  // - the animation is not resynced after the first iteration.
  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  let virtualTimeBase = 0;
  let totalElapsedTime = 0;
  let lastGrantedChunk = 0;
  let expiredCount = 0;

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    ++expiredCount;
    totalElapsedTime += lastGrantedChunk;
    testRunner.log(`Expired count: ${expiredCount}, `
        + `elaspedTime: ${totalElapsedTime}`);

    let grantVirtualTime = 500;
    let frameTimeTicks = virtualTimeBase + totalElapsedTime;

    if (expiredCount === 1 + 7
      || expiredCount === 1 + 7 + 5
      || expiredCount === 1 + 7 + 5 + 6) {
      // Animation starts when first screenshot is taken, so the first
      // screenshot should be blue. Screenshot #2 is taken on the third second
      // of the animation, so it should be yellow. Screenshot #3 is taken two
      // animation cycles later, so it should be yelloe again.
      const {result: {screenshotData}} =
          await dp.HeadlessExperimental.beginFrame(
              {frameTimeTicks, screenshot: {format: 'png'}});
      await logScreenShotInfo(screenshotData);
    } else {
      await dp.HeadlessExperimental.beginFrame(
          {frameTimeTicks, noDisplayUpdates: true});
    }

    // Grant more time or quit test.
    if (expiredCount < 20) {
      await dp.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending',
        budget: grantVirtualTime});
      lastGrantedChunk = grantVirtualTime;
    } else {
      testRunner.completeTest();
    }
  });

  // Pause for the first time and remember base virtual time.
  const {result: {virtualTimeTicksBase}} =
      await dp.Emulation.setVirtualTimePolicy(
            {initialVirtualTime: 100, policy: 'pause'});
  virtualTimeBase = virtualTimeTicksBase;

  // Renderer wants the very first frame to be fully updated.
  await dp.HeadlessExperimental.beginFrame({noDisplayUpdates: false,
      frameTimeTicks: virtualTimeBase});

  // Grant initial time.
  lastGrantedChunk = 500;

  // The loaded GIF is 100x100px and has 1 second of blue, 1 second of red and
  // 1 second of yellow.
  await dp.Page.navigate(
        {url: testRunner.url('/resources/compositor-image-animation.html')});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: lastGrantedChunk});

  async function AdvanceTime() {
    await dp.Emulation.onceVirtualTimeBudgetExpired();
    totalElapsedTime += lastGrantedChunk;
    testRunner.log(`Elasped time: ${totalElapsedTime}`);
    frameTimeTicks = virtualTimeBase + totalElapsedTime;
  }

  async function GrantMoreTime(budget) {
    lastGrantedChunk = budget;
    await dp.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending', budget});
  }

  function logScreenShotInfo(pngBase64) {
    const image = new Image();

    let callback;
    let promise = new Promise(fulfill => callback = fulfill);
    image.onload = function() {
      const canvas = document.createElement('canvas');
      canvas.width = image.naturalWidth;
      canvas.height = image.naturalHeight;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(image, 0, 0);
      const rgba = ctx.getImageData(0, 0, 1, 1).data;
      testRunner.log(`Screenshot rgba: ${rgba}`);
      callback();
    }

    image.src = `data:image/png;base64,${pngBase64}`;

    return promise;
  }
})
