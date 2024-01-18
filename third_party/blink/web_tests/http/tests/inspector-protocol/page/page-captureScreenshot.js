(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const SIZE = 50;
  const {page, session, dp} = await testRunner.startHTML(`
    <style>
    div {
      display: inline;
      width: 25px;
      height: 50px;
    }
    .left {
      background: green;
    }
    .right {
      background: red;
    }
    body {
      margin: 0;
    }
    </style>
    <div class=left>oo</div>
    <div class=right>oo</div>`, 'Tests that captureScreenshot works');

  async function processImageData(data, format) {
    const src = `data:image/${format};base64,${data}`;
    const expression = `
      (async () => {
        const image = new Image();
        await new Promise(fulfill => {
          image.onload = fulfill;
          image.src = '${src}';
        });

        const canvas = document.createElement('canvas');
        canvas.width = image.naturalWidth;
        canvas.height = image.naturalHeight;
        const context = canvas.getContext('2d');
        context.drawImage(image, 0, 0);
        const url = canvas.toDataURL('image/${format}');
        return {
          url,
          pixels: context.getImageData(0, 0, ${SIZE}, ${SIZE}).data,
        };
      })()
    `;
    const response = await dp.Runtime.evaluate({
      expression,
      awaitPromise: true,
      returnByValue: true,
    });
    const result = response.result.result.value;
    return result;
  }

  function getPixel(pixelData, x, y) {
    const offset = (x + y * SIZE) * 4;
    const pixel = {
      r: pixelData[offset],
      g: pixelData[offset + 1],
      b: pixelData[offset + 2],
      a: pixelData[offset + 3],
    }
    return pixel;
  }

  async function captureScreenshot(format) {
    const params = {
      format,
      clip: {x: 0, y: 0, width: SIZE, height: SIZE, scale: 1},
    };
    testRunner.log(JSON.stringify(params));
    const response = await dp.Page.captureScreenshot(params);
    const processedImageData =
      await processImageData(response.result.data, format);
    return processedImageData;
  }

  function colorSquareDiff(color1, color2) {
    const rDiff = color1.r - color2.r;
    const gDiff = color1.g - color2.g;
    const bDiff = color1.b - color2.b;
    const aDiff = color1.a - color2.a;
    return rDiff * rDiff + gDiff * gDiff + bDiff * bDiff + aDiff * aDiff;
  }

  async function compare(processedImageData, expectedPixels) {
    let numBadPixels = 0;
    for (let x = 0; x < SIZE; x++) {
      for (let y = 0; y < SIZE; y++) {
        const pixel = getPixel(processedImageData.pixels, x, y);
        const expectedPixel = getPixel(expectedPixels, x, y);
        const diff = colorSquareDiff(pixel, expectedPixel);
        if (diff > 25 * 25) {
          numBadPixels += 1;
        }
      }
    }

    const percentBad = numBadPixels / (SIZE * SIZE);
    testRunner.log(`less than 20% bad pixels: ${percentBad < 0.2}`);
  }

  const png = await captureScreenshot('png');

  // Confirm green and red pixels.
  const green = getPixel(png.pixels, 10, 0);
  const red = getPixel(png.pixels, 35, 0);
  testRunner.log(`green: ${JSON.stringify(green)}`);
  testRunner.log(`red: ${JSON.stringify(red)}`);

  // Compare lossy images against the (non-lossy) PNG pixels.
  const jpeg = await captureScreenshot('jpeg');
  compare(jpeg, png.pixels);
  const webp = await captureScreenshot('webp');
  compare(webp, png.pixels);

  testRunner.completeTest();
})
