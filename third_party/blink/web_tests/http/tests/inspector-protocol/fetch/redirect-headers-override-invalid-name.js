(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
    `Tests that invalid header name results in an error`,
  );

  await dp.Fetch.enable();
  const finalUrl = testRunner.url(
    '../network/resources/echo-headers.php?headers=X-DevTools-Test@',
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
        // Invalid header name according to net::HttpUtil::IsValidHeaderName.
        { name: 'X-DevTools-Test@', value: 'bar=bazz' },
      ],
    }),
  );
  dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
  });
  const afterRedirect = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(
    'X-DevTools-Test@: after redirect:' +
      (afterRedirect.request.headers['X-DevTools-Test@'] ?? 'unset'),
  );
  dp.Fetch.continueRequest({
    requestId: afterRedirect.requestId,
  });
  testRunner.log(await contentPromise);
  testRunner.completeTest();
});
