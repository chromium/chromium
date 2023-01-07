// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests compositor animated css handling.');

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
    testRunner.log(`Expired count: ${expiredCount}`
        + `, elaspedTime: ${totalElapsedTime}`);

    let grantVirtualTime = 500;
    let frameTimeTicks = virtualTimeBase + totalElapsedTime;

    if (expiredCount == 1) {
      // Renderer wants the very first frame to be fully updated.
      await dp.HeadlessExperimental.beginFrame({frameTimeTicks});
    } else {
      if (expiredCount >= 4 && expiredCount <= 6) {
        // Issue updateless frames.
        await dp.HeadlessExperimental.beginFrame(
            {frameTimeTicks, noDisplayUpdates: true});
      } else {
        // Update frame and grab a screenshot, logging background color.
        const {result: {screenshotData}} =
            await dp.HeadlessExperimental.beginFrame(
                {frameTimeTicks, screenshot: {format: 'png'}});
        await logScreenShotInfo(screenshotData);
      }
    }

    // Grant more time or quit test.
    if (expiredCount < 10) {
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
      await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  virtualTimeBase = virtualTimeTicksBase;

  lastGrantedChunk = 500;

  // Animates opacity of a blue 100px square on red blackground over 4
  // seconds (1.0 -> 0 -> 1.0 four times). Logs events to console.
  //
  // Timeline:
  //      0 ms:  --- animation starts at 500ms ---
  //    500 ms:  1.0 opacity  -> blue background.
  //   1000 ms:    0 opacity  ->  red background.
  //   1500 ms:  1.0 opacity  -> blue background.
  //   2000 ms:    0 opacity  ->  red background.
  //   2500 ms:  1.0 opacity  -> blue background.
  //   3000 ms:    0 opacity  ->  red background.
  //   3500 ms:  1.0 opacity  -> blue background.
  //   4000 ms:    0 opacity  ->  red background.
  //   4500 ms:  1.0 opacity  -> blue background.
  //
  // The animation will start with the first BeginFrame after load.
  await dp.Page.navigate(
      {url: testRunner.url('/resources/compositor-css-animation.html')});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: lastGrantedChunk});

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
