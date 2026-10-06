(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that fetch() requests whose body has been received finish loading.`);

  await dp.Network.enable();

  async function logLoadingResult(title, expression) {
    testRunner.log('');
    testRunner.log(title);
    const pagePromise = session.evaluateAsync(expression);
    const event = await Promise.race([
      dp.Network.onceLoadingFinished(),
      dp.Network.onceLoadingFailed(),
    ]);
    testRunner.log(`Page: ${await pagePromise}`);
    testRunner.log(`Event: ${event.method}${
        event.params.errorText ? ' ' + event.params.errorText : ''}`);
    const response =
        await dp.Network.getResponseBody({requestId: event.params.requestId});
    testRunner.log(`Body: ${
        response.error ? 'error: ' + response.error.message :
                         `'${response.result.body}'`}`);
  }

  // resource.php is no-store, so the body is not buffered; tail_wait delays
  // the end of the body after the data is sent.
  await logLoadingResult(
      'Scenario 1: No-store body read to the end with a stream reader',
      `(async () => {
    const response =
        await fetch('/devtools/network/resources/resource.php?tail_wait=100');
    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let text = '';
    while (true) {
      const {done, value} = await reader.read();
      if (done)
        return text;
      text += decoder.decode(value, {stream: true});
    }
  })()`);

  // The body ends before the buffering of an unread body starts.
  const emptyUrl = testRunner.url('./resources/content-length-0.pl');
  await logLoadingResult(
      'Scenario 2: Empty body that is never read',
      `fetch(${JSON.stringify(emptyUrl)}).then(r => r.status)`);

  testRunner.completeTest();
})
