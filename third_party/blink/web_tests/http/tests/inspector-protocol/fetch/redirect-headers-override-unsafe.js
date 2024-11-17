(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that unsafe headers result in an error`);

  await dp.Fetch.enable();

  const final_url = 'http://127.0.0.1:8000/inspector-protocol/network/resources/echo-headers.php?headers=Cookie2';
  const contentPromise = session.evaluateAsync(`
    fetch('http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl?${final_url}').then(r => r.text())
  `);

  const beforeRedirect = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(await dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
    headers: [
      // One of the unsafe headers according to network::IsRequestHeaderSafe.
      {name: 'Cookie2', value: 'bar=bazz'}
    ]
  }));
  dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
  });
  const afterRedirect = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log('Cookie2 after redirect:' + (afterRedirect.request.headers['cookie2'] ?? 'unset'));
  dp.Fetch.continueRequest({
    requestId: afterRedirect.requestId,
  });
  testRunner.log(await contentPromise);
  testRunner.completeTest();
})
