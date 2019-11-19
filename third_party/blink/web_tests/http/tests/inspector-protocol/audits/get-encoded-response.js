(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests encoding of a response body`);

  await dp.Network.enable();

  function approximate(value, granularity) {
    return typeof value === 'number' ? Math.round(value / granularity) * granularity : value;
  }

  async function logResponse(url, encoding, quality, sizeOnly) {
    testRunner.log(`\nResults for ${url} encoding=${encoding} q=${quality} sizeOnly=${sizeOnly}`);

    session.evaluate(`fetch(${JSON.stringify(url)}).then(r => r.text())`);

    const requestId = (await dp.Network.onceResponseReceived()).params.requestId;
    const result = (await dp.Audits.getEncodedResponse({requestId, encoding, quality, sizeOnly})).result;

    if (!result) {
      testRunner.log('failed to determine');
      return;
    }

    const length = result.body && result.body.length;
    const encodedSize = result.encodedSize;
    testRunner.log(`body=${typeof result.body} body.length~${approximate(length, 100)}`);
    testRunner.log(`original=${result.originalSize} encoded~${approximate(encodedSize, 100)}`);
  }

  await logResponse("/resources/square200.png", "jpeg");
  await logResponse("/resources/square200.png", "webp", .8, false);

  await logResponse("/resources/square200.png", "jpeg", 1, true);
  await logResponse("/resources/square200.png", "jpeg", .5, true);

  await logResponse("/resources/square20.bmp", "jpeg", .8, true);
  await logResponse("/resources/square20.bmp", "png");

  await logResponse("/resources/load-and-stall.php?name=dummy.html&mimeType=image%2Fpng", "png");

  testRunner.completeTest();
})
