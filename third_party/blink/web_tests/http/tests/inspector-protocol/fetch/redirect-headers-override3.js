(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests overridden headers don't stick across redirects`);

  await dp.Fetch.enable();

  // Note Referer still sticks due to implementation limitations.
  const final_url = 'http://127.0.0.1:8000/inspector-protocol//network/resources/echo-headers.php?headers=HTTP_X_DEVTOOLS_TEST:HTTP_COOKIE:HTTP_REFERER';
  const intermediate_url = `http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl?${final_url}`;
  const contentPromise = session.evaluateAsync(`
    fetch('http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl?${intermediate_url}').then(r => r.text())
  `);

  const beforeRedirect = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
    headers: [
      {name: 'X-Devtools-Test', value: 'foo'},
      {name: 'Cookie', value: 'name=foo'}
    ]
  });
  const afterRedirect1 = (await dp.Fetch.onceRequestPaused()).params;
  const stabilizeNames = [...TestRunner.stabilizeNames, 'User-Agent'];
  testRunner.log(afterRedirect1.request.headers, 'Redirected request 1 headers: ', stabilizeNames);
  dp.Fetch.continueRequest({
    requestId: afterRedirect1.requestId,
    headers: [
      {name: 'X-Devtools-Test', value: 'bar'},
      {name: 'Cookie', value: 'name=bar'}
    ]
  });

  const afterRedirect2 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(afterRedirect2.request.headers, 'Redirected request 2 headers: ', stabilizeNames);
  dp.Fetch.continueRequest({
    requestId: afterRedirect2.requestId,
  });
  testRunner.log(await contentPromise);
  testRunner.completeTest();
})
