(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Ensures that data and header length sent from protocol is proper sizes`);

  // When chunk encoded the last chunk will always be 5 bytes '0\r\n\r\n' and
  // we do not receive a dataReceived event and instead it's in loadingFinished event.
  const HTTP_CLOSING_CHUNK_SIZE = 5;

  var requestsMap = new Map();
  var pendingRequests = 0;

  function sendRequest(url) {
    dp.Runtime.evaluate({expression: `fetch('${url}').then(r => r.text())`});
    pendingRequests++;
  }

  function printResults() {
    var requests = Array.from(requestsMap.values());
    requests.sort((a, b) => a.url < b.url ? 1 : -1);
    testRunner.log('');
    for (var request of requests) {
      testRunner.log('url: ' + request.url);
      testRunner.log('  isChunked: ' + request.isChunked);
      testRunner.log('  isH2: ' + request.isH2);
      testRunner.log('  redirected: ' + request.redirected);
      testRunner.log('  headersSize: ' + request.headersSize);
      testRunner.log('  receivedDataSize: ' + request.receivedDataSize);
      if (!request.redirected) // reportedTotalSize is not stable across platforms.
        testRunner.log('  reportedTotalSize: ' + request.reportedTotalSize);
      testRunner.log('');
    }
  }

  testRunner.log('Test started');

  dp.Network.onRequestWillBeSent(event => {
    var params = event.params;
    if (requestsMap.has(params.requestId)) {
      // is redirect.
      var request = requestsMap.get(params.requestId);
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
    var params = event.params;
    var isH2 = params.response.protocol === 'h2';
    var request = requestsMap.get(params.requestId);
    request.isChunked = isH2 || (params.response.headers['Transfer-Encoding'] === 'chunked');
    request.isH2 = isH2;
    request.headersSize = params.response.encodedDataLength;
  });

  dp.Network.onLoadingFinished(event => {
    var params = event.params;
    var request = requestsMap.get(params.requestId);
    request.reportedTotalSize += params.encodedDataLength;
    pendingRequests--;
    if (pendingRequests <= 0) {
      printResults();
      testRunner.completeTest();
    }
  });

  dp.Network.onDataReceived(event => {
    var params = event.params;
    var request = requestsMap.get(params.requestId);
    request.receivedDataSize += params.encodedDataLength;
  });

  await dp.Network.enable();
  testRunner.log('Network agent enabled');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'redirect=1');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'cached=1');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'size=4&' +
              'flush_header_with_x_bytes=1&' +
              'wait_after_headers_packet=25&' +
              'flush_every=1&' +
              'wait_every_x_bytes=1&' +
              'wait_duration_every_x_bytes=25');
  sendRequest('/inspector-protocol/resources/data-xfer-resource.php?' +
              'size=4&' +
              'flush_header_with_x_bytes=1&' +
              'wait_after_headers_packet=25&' +
              'flush_every=1&' +
              'wait_every_x_bytes=1&' +
              'wait_duration_every_x_bytes=25');
})
