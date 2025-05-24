(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
    `Tests that invalid header value results in an error`,
  );

  await dp.Fetch.enable();
  const finalUrl = testRunner.url(
    '../network/resources/echo-headers.php?headers=Content-Type',
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
        // Invalid header value according to net::HttpUtil::IsValidHeaderValue.
        { name: 'Content-Type', value: 'plain\ntext' },
      ],
    }),
  );
  dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
  });
  const afterRedirect = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(
    'X-DevTools-Test:  after redirect:' +
      (afterRedirect.request.headers['X-DevTools-Test: '] ?? 'unset'),
  );
  dp.Fetch.continueRequest({
    requestId: afterRedirect.requestId,
  });
  testRunner.log(await contentPromise);
  testRunner.completeTest();
});
