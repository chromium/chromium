(async function (testRunner) {
  const { session, dp } = await testRunner.startBlank(
    'Tests that Fetch.continueRequest correctly handles initiator path for cross-port localhost redirects.');

  await dp.Page.enable();
  await dp.Fetch.enable();

  const redirectUrl = 'http://127.0.0.1:8080/inspector-protocol/resources/cors-data.php';

  async function runTest(pageUrl, description) {
    testRunner.log(`\n--- Scenario: ${description} ---`);

    const mainDocumentRequestPromise = dp.Fetch.onceRequestPaused(
      ({ params }) => params.request.url === pageUrl);
    const fetchRequestPromise = dp.Fetch.onceRequestPaused(
      ({ params }) => params.request.url.endsWith('/api-call'));

    dp.Page.navigate({ url: pageUrl });

    const { params: { requestId: mainDocumentRequestId } } = await mainDocumentRequestPromise;
    dp.Fetch.continueRequest({ requestId: mainDocumentRequestId });
    await dp.Page.onceLoadEventFired();

    const fetchResultPromise = session.evaluateAsync(`
      (async () => {
        try {
          const response = await fetch('/api-call');
          const text = await response.text();
          return 'Success: Response text is "' + text + '"';
        } catch (e) {
          return 'Failed: ' + e.toString();
        }
      })()
    `);

    const { params: { requestId: fetchRequestId } } = await fetchRequestPromise;
    testRunner.log(`  Redirecting fetch request to: ${redirectUrl}`);
    dp.Fetch.continueRequest({
      requestId: fetchRequestId,
      url: redirectUrl
    });

    const result = await fetchResultPromise;
    testRunner.log(`  Verification result: ${result}`);
  }

  await runTest(
    'http://127.0.0.1:8000/',
    "Request from root document ('/')"
  );

  await runTest(
    'http://127.0.0.1:8000/?foo=bar',
    "Request from root document with a query parameter"
  );

  await runTest(
    'http://127.0.0.1:8000/inspector-protocol/resources/empty.html',
    "Request from document with a path"
  );

  testRunner.completeTest();
})