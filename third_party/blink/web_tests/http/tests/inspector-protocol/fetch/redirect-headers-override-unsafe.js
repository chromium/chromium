(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
    `Tests that unsafe headers results in an error`,
  );

  await dp.Fetch.enable();

  const finalUrl = testRunner.url(
    '../network/resources/echo-headers.php?headers=Cookie2',
  );
  const redirectUrl = testRunner.url(
    `../fetch/resources/redirect.pl?${finalUrl}`,
  );
  const contentPromise = session.evaluateAsync((redirect) => {
    return fetch(redirect).then((r) => r.text());
  }, redirectUrl);

  const beforeRedirect = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(
    await dp.Fetch.continueRequest({
      requestId: beforeRedirect.requestId,
      headers: [
        // One of the unsafe headers name according to network::IsRequestHeaderSafe.
        { name: 'Cookie2', value: 'bar=bazz' },
      ],
    }),
  );
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
