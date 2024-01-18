(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that resource bodies can be taken as streams.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Network.setRequestInterception({patterns: [
    {urlPattern: '*', interceptionStage: 'HeadersReceived'}
  ]});

  const bodyPattern = 'The_quick_brown_fox_jumps_over_the_lazy_dog_0123456789';

  function checkData(str, pattern) {
    for (let i = 0; i < Math.ceil(str.length / pattern.length); ++i) {
      const start = i * pattern.length;
      const length = Math.min(pattern.length, str.length - start);
      const expected = pattern.substr(0, length);
      const fragment = str.substr(start, length);
      if (expected != fragment) {
        testRunner.log(`Data mistmatch near offset ${start}, got ${fragment}, expected: ${expected}`);
        break;
      }
    }
  }

  let lastInterceptionId;
  let lastStreamId;
  async function startRequestAndTakeStream(size, params) {
    cancelAndClose();
    params = params || {};
    params["size"] = size;
    params["body_pattern"] = bodyPattern;
    let paramStr = Object.keys(params).map(k => `${k}=${params[k]}`).join('&');
    const url = `/devtools/network/resources/resource.php?${paramStr}`;
    session.evaluate(`fetch("${url}");`);
    const intercepted = (await dp.Network.onceRequestIntercepted()).params;
    lastInterceptionId = intercepted.interceptionId;
    let response = await dp.Network.takeResponseBodyForInterceptionAsStream({interceptionId: lastInterceptionId});
    if (response.error) {
      testRunner.log(`Error taking stream: ${response.error.message}`);
      return;
    }
    lastStreamId = response.result.stream;
    return lastStreamId;
  }

  function cancelAndClose() {
    if (lastInterceptionId) {
      dp.Network.continueInterceptedRequest({interceptionId: lastInterceptionId, errorReason: 'Aborted'});
      lastInterceptionId = undefined;
    }
    if (lastStreamId) {
      dp.IO.close({handle: lastStreamId});
      lastStreamId = undefined;
    }
  }

  testRunner.runTestSuite([
    async function testBasicUsage() {
      const stream = await startRequestAndTakeStream(100);
      if (!stream)
        return;
      let result = (await dp.IO.read({handle: stream, size: 100})).result;
      testRunner.log(`data: ${result.data} (${result.data.length}) eof: ${result.eof}`);
      result = (await dp.IO.read({handle: stream, size: 100})).result;
      testRunner.log(`eof: ${result.eof}`);
    },

    async function testLargeRead() {
      const stream = await startRequestAndTakeStream(100000, {chunked: true});
      if (!stream)
        return;
      let result = (await dp.IO.read({handle: stream, size: 100001})).result;
      testRunner.log(`data: (${result.data.length}) eof: ${result.eof}`);
      checkData(result.data, bodyPattern);
      result = (await dp.IO.read({handle: stream, size: 100001})).result;
      testRunner.log(`eof: ${result.eof}`);
    },

    async function testSmallNonOverlappingReads() {
      const stream = await startRequestAndTakeStream(10000, {chunked: true});
      if (!stream)
        return;
      let data = '';
      for (;;) {
        const response = (await dp.IO.read({handle: stream, size: 333})).result;
        data += response.data;
        if (response.eof)
          break;
      }
      testRunner.log(`data: (${data.length})`);
      checkData(data, bodyPattern);
    },

    async function testSmallOverlappingReads() {
      const streamSize = 100000;
      const readSize = 333;
      const stream = await startRequestAndTakeStream(streamSize, {chunked: true});
      if (!stream)
        return;
      let data = await new Promise(fulfill => {
        let data = '';
        let pendingReads = 0;
        function onResponse(response) {
          --pendingReads;
          data += response.data;
          if (response.eof) {
            fulfill(data);
            return;
          }
          for (;pendingReads < 4; ++pendingReads)
            dp.IO.read({handle: stream, size: readSize}).then(r => onResponse(r.result));
        }
        for (;pendingReads < 4; ++pendingReads)
          dp.IO.read({handle: stream, size: readSize}).then(r => onResponse(r.result));
      });
      testRunner.log(`data: (${data.length})`);
      checkData(data, bodyPattern);
    },

    async function testRead0() {
      const stream = await startRequestAndTakeStream(100);
      if (!stream)
        return;
      const {error} = (await dp.IO.read({handle: stream, size: 0}));
      testRunner.log(`reading 0: ${error.message}`);
      const result = (await dp.IO.read({handle: stream, size: 100})).result;
      testRunner.log(`data: ${result.data} (${result.data.length}) eof: ${result.eof}`);
    },

    async function testTakeTwice() {
      const stream = await startRequestAndTakeStream(100);
      if (!stream)
        return;
      let response = await dp.Network.takeResponseBodyForInterceptionAsStream({interceptionId: lastInterceptionId});
      testRunner.log(`Trying to take stream twice: ${response.error.message}`);
    },

    async function testReadAfterCancel() {
      const stream = await startRequestAndTakeStream(100);
      if (!stream)
        return;
      dp.Network.continueInterceptedRequest({interceptionId: lastInterceptionId, errorReason: 'Aborted'});
      lastInterceptionId = undefined;
      let result = (await dp.IO.read({handle: stream, size: 100})).result;
      testRunner.log(`data: ${result.data} (${result.data.length}) eof: ${result.eof}`);
      result = (await dp.IO.read({handle: stream, size: 100})).result;
      testRunner.log(`eof: ${result.eof}`);
    },

    async function testContinueAfterBodyTaken() {
      const stream = await startRequestAndTakeStream(100);
      if (!stream)
        return;
      const response = await dp.Network.continueInterceptedRequest({interceptionId: lastInterceptionId});
      testRunner.log(`Attempting to continue as is after taking request: ${response.error.message}`);
    },

    async function testReadAfterClose() {
      const stream = await startRequestAndTakeStream(100);
      if (!stream)
        return;
      dp.IO.close({handle: stream});
      const response = await dp.IO.read({handle: stream, size: 100});

      testRunner.log(`Attempting read after close: ${response.error.message}`);
    },

  ]);
})
