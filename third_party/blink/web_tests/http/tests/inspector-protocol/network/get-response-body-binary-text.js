(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that binary data served as text/plain is returned as base64.`);

  await dp.Network.enable();

  async function getBody(url) {
    session.evaluate(`fetch(${JSON.stringify(url)}).then(r => r.text());`);
    const event = await dp.Network.onceRequestWillBeSent();
    await dp.Network.onceLoadingFinished(
        e => e.params.requestId === event.params.requestId);
    return dp.Network.getResponseBody(
        {requestId: event.params.requestId});
  }

  // Binary data with text/plain; charset=utf-8 must be base64 encoded because
  // TextResourceDecoder replaces invalid byte sequences with U+FFFD.
  testRunner.log('Binary data served as text/plain; charset=utf-8:');
  const binary = await getBody(
      testRunner.url('./resources/get-response-body-binary-text.php'));
  testRunner.log('base64Encoded: ' + binary.result.base64Encoded);

  // Valid UTF-8 text with text/plain should be returned as plain text.
  testRunner.log('Valid UTF-8 text served as text/plain; charset=utf-8:');
  const text = await getBody(
      testRunner.url('./resources/get-response-body-valid-text.php'));
  testRunner.log('base64Encoded: ' + text.result.base64Encoded);
  testRunner.log('body: ' + text.result.body);

  testRunner.completeTest();
})
