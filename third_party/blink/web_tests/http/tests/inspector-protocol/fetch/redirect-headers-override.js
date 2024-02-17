(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests overridden headers don't stick across redirects`);

  await dp.Fetch.enable();

  // Note Referer still sticks due to implementation limitations.
  const final_url = 'http://127.0.0.1:8000/inspector-protocol//network/resources/echo-headers.php?headers=HTTP_X_DEVTOOLS_TEST:HTTP_COOKIE:HTTP_REFERER';
  const contentPromise = session.evaluateAsync(`
    fetch('http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl?${final_url}').then(r => r.text())
  `);

  const beforeRedirect = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
    headers: [
      {name: 'X-Devtools-Test', value: 'foo'},
      {name: 'Cookie', value: 'bar=bazz'}
    ]
  });
  const afterRedirect = (await dp.Fetch.onceRequestPaused()).params;
  const stabilizeNames = [...TestRunner.stabilizeNames, 'User-Agent'];
  testRunner.log(afterRedirect.request.headers, 'Redirected request headers:', stabilizeNames);
  dp.Fetch.continueRequest({
    requestId: afterRedirect.requestId,
  });
  testRunner.log(await contentPromise);
  testRunner.completeTest();
})
