(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Ensures that data recorded in durable http response are proper sizes`);

  const requestsMap = new Map();
  let pendingRequests = 0;

  function sendRequest(url) {
    dp.Runtime.evaluate({expression: `fetch('${url}').then(r => r.text())`});
    pendingRequests++;
  }

  async function printResults() {
    const requestIds = Array.from(requestsMap.keys());
    requestIds.sort((a, b) => requestsMap.get(a).url.localeCompare(requestsMap.get(b).url));
    testRunner.log('');
    for (const requestId of requestIds) {
      const request = requestsMap.get(requestId);

      testRunner.log('url: ' + request.url);
      testRunner.log('  isChunked: ' + request.isChunked);
      testRunner.log('  redirected: ' + request.redirected);
      // Retrieve body from durable http message storage
      const data = await dp.Network.getResponseBody({requestId});
      testRunner.log('  durableMessage size: ' + (data.result ? data.result.body.length : 'absent'));
      testRunner.log('  payload: ' + JSON.stringify(data?.result));
      testRunner.log('');
    }
  }

  testRunner.log('Test started');

  dp.Network.onRequestWillBeSent(event => {
    const params = event.params;
    if (requestsMap.has(params.requestId)) {
      // is redirect.
      const request = requestsMap.get(params.requestId);
      request.reportedTotalSize += params.redirectResponse.encodedDataLength;
      request.redirected = true;
      // This is to store it, but not reuse it.
      requestsMap.set(Symbol(params.requestId), request);
    }
    requestsMap.set(params.requestId, {
      url: params.request.url,
      isChunked: null,
      isH2: null,
      headersSize: 0,
      receivedDataSize: 0,
      reportedTotalSize: 0,
      redirected: false
    });
  });

  dp.Network.onResponseReceived(event => {
    const params = event.params;
    const isH2 = params.response.protocol === 'h2';
    const request = requestsMap.get(params.requestId);
    request.isChunked = isH2 || (params.response.headers['Transfer-Encoding'] === 'chunked');
    request.isH2 = isH2;
    request.headersSize = params.response.encodedDataLength;
  });

  dp.Network.onLoadingFinished(async event => {
    const params = event.params;
    const request = requestsMap.get(params.requestId);
    request.reportedTotalSize += params.encodedDataLength;
    pendingRequests--;
    if (pendingRequests <= 0) {
      await printResults();
      testRunner.completeTest();
    }
  });

  dp.Network.onDataReceived(event => {
    const params = event.params;
    const request = requestsMap.get(params.requestId);
    request.receivedDataSize += params.encodedDataLength;
  });

  await dp.Network.enable({maxTotalBufferSize: 4500, enableDurableMessages: true});
  testRunner.log('Network agent enabled');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'cached=1');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'size=4000&' +
              'flush_header_with_x_bytes=1&' +
              'wait_after_headers_packet=25&' +
              'flush_every=1000&' +
              'wait_every_x_bytes=100&' +
              'wait_duration_every_x_bytes=25');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'size=4&' +
              'flush_header_with_x_bytes=1&' +
              'wait_after_headers_packet=25&' +
              'flush_every=1&' +
              'wait_every_x_bytes=1&' +
              'wait_duration_every_x_bytes=25');
})