(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that screenshot encoding failures return an error');

  // CPU compositing avoids SwiftShader's 8192-pixel framebuffer limit.
  await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 100,
    deviceScaleFactor: 1,
    mobile: false,
  });

  const dimensions = [[16383, 100], [16384, 100], [100, 16383], [100, 16384]];
  for (const [width, height] of dimensions) {
    for (const format of ['webp', 'png', 'jpeg']) {
      testRunner.log(`${format} ${width}x${height}`);
      const response = await dp.Page.captureScreenshot({
        format,
        quality: 100,
        clip: {x: 0, y: 0, width, height, scale: 1},
        captureBeyondViewport: true,
      });
      if (response.error) {
        testRunner.log(response.error);
      } else {
        const imageSize = await session.evaluateAsync(async (data, format) => {
          const image = new Image();
          image.src = `data:image/${format};base64,${data}`;
          await image.decode();
          return `${image.naturalWidth}x${image.naturalHeight}`;
        }, response.result.data, format);
        testRunner.log(`Image size: ${imageSize}`);
      }
    }
  }

  testRunner.completeTest();
})
