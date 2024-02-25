(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that we intercept errors reported instead of response when intercepting responses.`);

  function isResponse(params) {
    return "responseErrorReason" in params || "responseStatusCode" in params;
  }

  async function requestAndDump(expectResponse) {
    const url = '/devtools/network/resources/resource-deny.php';
    const responsePromise = session.evaluateAsync(`
        fetch("${url}").then(r => r.text()).
                        catch(error => 'Error: ' + JSON.stringify(error))
    `).then(response => testRunner.log(`Fetched responsed with: ${response}`));

    const requestParams = (await dp.Fetch.onceRequestPaused()).params;
    if (!requestParams.request.url.endsWith(url)) {
      testRunner.fail(`Paused at wrong request, got ${requestParams.request.url}, expected ${url}`);
      return;
    }
    if (isResponse(requestParams)) {
      testRunner.fail(`Paused at wrong phase, expected request, got response`);
      return;
    }
    dp.Fetch.continueRequest({requestId: requestParams.requestId});

    if (expectResponse) {
      const responseParams = (await dp.Fetch.onceRequestPaused()).params;
      const phase = isResponse(responseParams) ? "response" : "request";
      const message = `Intercepted ${responseParams.request.method} ${responseParams.request.url} at ${phase}`;
      const maybeError = responseParams.responseErrorReason ? `, error: ${responseParams.responseErrorReason}` : '';
      testRunner.log(message + maybeError);
    }
    return {requestId: requestParams.requestId, responsePromise};
  }

  // This browser-level FetchHelper is only for mocking error responses to the
  // Fetch handler under test, which is on the renderer target (and gets
  // inserted on top of the browser one).
  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  await helper.enable();
  helper.onceRequest().continueRequest();
  helper.setLogPrefix("[mock fetcher] ");

  dp.Page.enable();
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();

  helper.onRequest(/resource-deny/).fail({
    errorReason: 'AccessDenied'
  });

  await dp.Fetch.enable({patterns: [{requestStage: 'Request'}, {requestStage: 'Response'}]});

  {
    testRunner.log(`Testing continuing failed request...`);
    const {requestId, responsePromise} = await requestAndDump(true);
    dp.Fetch.continueRequest({requestId});
    await responsePromise;
  }

  {
    testRunner.log(`Testing failing failed request...`);
    const {requestId, responsePromise} = await requestAndDump(true);
    dp.Fetch.failRequest({requestId, errorReason: 'Aborted'});
    await responsePromise;
  }

  {
    testRunner.log(`Testing fulfilling failed request...`);
    const {requestId, responsePromise} = await requestAndDump(true);
    dp.Fetch.fulfillRequest({
      requestId,
      responseCode: 200,
      responseHeaders: [],
      body: btoa("overriden response body")
    });
    await responsePromise;
  }

  {
    testRunner.log(`Testing we're not pausing on errors when only intercepting requests...`);
    await dp.Fetch.enable({patterns: [{requestStage: 'Request'}]});
    dp.Fetch.onRequestPaused(event => {
      if (isResponse(event.params))
        testRunner.fail(`Unexpected Fetch.requestPaused event for error response`);
    });
    const {responsePromise} = await requestAndDump(false);
    await responsePromise;
  }
  testRunner.completeTest();
})
