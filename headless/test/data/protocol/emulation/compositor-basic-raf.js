// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests compositor basic rAF operation.');

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  let virtualTimeBase = 0;
  let totalElapsedTime = 0;
  let frameTimeTicks = 0;
  let lastGrantedChunk = 1000;

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate(
      {url: testRunner.url('/resources/compositor-basic-raf.html')});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: lastGrantedChunk});

  // Renderer wants the very first frame to be fully updated.
  await AdvanceTime();
  await session.evaluate('startRAF()');
  await dp.HeadlessExperimental.beginFrame({frameTimeTicks});
  await GrantMoreTime(100);

  // Send 3 updateless frames.
  for (var n = 0; n < 3; ++n) {
    await AdvanceTime();
    await dp.HeadlessExperimental.beginFrame({frameTimeTicks,
        noDisplayUpdates: true});
    await GrantMoreTime(100);
  }

  // Grab screenshot, expected size 800x600, rgba: 0,0,50,255.
  await AdvanceTime();
  testRunner.log(await session.evaluate('displayRAFCount();'));
  const screenshotData =
      (await dp.HeadlessExperimental.beginFrame(
          {frameTimeTicks, screenshot: {format: 'png'}}))
      .result.screenshotData;
  await logScreenShotInfo(screenshotData);

  testRunner.completeTest();

  async function AdvanceTime() {
    await dp.Emulation.onceVirtualTimeBudgetExpired();
    totalElapsedTime += lastGrantedChunk;
    testRunner.log(`Elapsed time: ${totalElapsedTime}`);
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
      testRunner.log(`Screenshot size: `
          + `${image.naturalWidth} x ${image.naturalHeight}`);
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
