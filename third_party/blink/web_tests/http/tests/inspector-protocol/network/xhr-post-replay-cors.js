(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'http://localhost:8000/',
      'Verifies that replayed CORS XHRs still have post data');
  await dp.Network.enable();

  const requestsById = {};
  let sentReplayXhr = false;

  function replayOptionsXhr() {
    sentReplayXhr = true;
    dp.Network.replayXHR({requestId: Object.keys(requestsById)[0]});
  }

  function printResultsAndFinish() {
    const requests = Object.values(requestsById)
                         .sort((one, two) => one.wallTime - two.wallTime)
                         .map(request => {
                           delete request.wallTime;
                           return request;
                         });

    // Ignore OPTIONS preflight requests.
    // TODO(crbug.com/941297): Add the OPTIONS request back to the test results once this bug is fixed.
    let postIndex = 0;
    for (const request of requests) {
      if (request.method !== 'POST')
        continue;
      testRunner.log(`POST request ${postIndex++}: ${JSON.stringify(request, null, 2)}`);
    }
    testRunner.completeTest();
  }

  dp.Network.onRequestWillBeSent(event => {
    requestsById[event.params.requestId] = {
      method: event.params.request.method,
      url: event.params.request.url,
      postData: event.params.request.postData,
      wallTime: event.params.wallTime
    };
  });

  dp.Network.onLoadingFinished(async event => {
    const requestId = event.params.requestId;
    const responseData =
        await dp.Network.getResponseBody({'requestId': requestId});
    requestsById[requestId].responseData = responseData.result.body;

    if (Object.values(requestsById).every(request => request.responseData)) {
      if (sentReplayXhr)
        printResultsAndFinish();
      else
        replayOptionsXhr();
    }
  });

  await session.evaluate(`
      const xhr = new XMLHttpRequest();
      xhr.open('POST', 'http://127.0.0.1:8000/inspector-protocol/network/resources/cors-return-post.php');
      xhr.setRequestHeader('content-type', 'application/json');
      xhr.send(JSON.stringify({data: 'test post data'}));
  `);
})
