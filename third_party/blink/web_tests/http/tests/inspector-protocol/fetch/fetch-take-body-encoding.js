(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that non-utf8 content is returned base64-encoded`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Fetch.enable({
    patterns: [{
      requestStage: 'Response',
    }]
  });

  let lastRequestId;
  let lastStreamId;
  async function startRequestAndTakeStream(charset) {
    cancelAndClose();
    const url = testRunner.url(`../network/resources/charset.php?test=${charset}`);
    session.evaluate(`fetch("${url}");`);
    const intercepted = (await dp.Fetch.onceRequestPaused()).params;
    lastRequestId = intercepted.requestId;
    let response = await dp.Fetch.takeResponseBodyAsStream({requestId: lastRequestId});
    if (response.error) {
      testRunner.log(`Error taking stream: ${response.error.message}`);
      return;
    }
    lastStreamId = response.result.stream;
    return lastStreamId;
  }

  function cancelAndClose() {
    if (lastRequestId) {
      dp.Fetch.failRequest({requestId: lastRequestId, errorReason: 'Aborted'});
      lastRequestId = undefined;
    }
    if (lastStreamId) {
      dp.IO.close({handle: lastStreamId});
      lastStreamId = undefined;
    }
  }

  async function runTestCase(test, size) {
    const stream = await startRequestAndTakeStream(test);
    if (!stream)
      return;
    const result = (await dp.IO.read({ handle: stream, size: size ?? 20 })).result;
    testRunner.log(`data: ${result.data} base64: ${result.base64Encoded}`);
  };

  testRunner.runTestSuite([
    runTestCase.bind(null, 'windows-1251'),
    runTestCase.bind(null, 'utf-8'),
    runTestCase.bind(null, 'binary-utf8'),
    runTestCase.bind(null, 'binary'),
    runTestCase.bind(null, 'utf-8-mb', 20), // cuts a utf-8 char in the middle.
    runTestCase.bind(null, 'utf-8-mb', 21),
  ]);
})
