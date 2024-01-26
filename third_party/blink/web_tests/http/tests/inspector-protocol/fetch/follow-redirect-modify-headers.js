(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that client can modify headers when continuing following a redirect response.`);

  await dp.Fetch.enable({});
  const bodyPromise = session.evaluateAsync(`(function () {
    const headers = new Headers();
    headers.append('X-DevToolsTest1', 'This will be removed');
    headers.append('X-DevToolsTest2', 'This will be set to empty');
    headers.append('X-DevToolsTest3', 'This will be replaced');
    headersToDump = [...headers.keys(), 'X-DevToolsTest4'].map(header => 'HTTP_' + header.toUpperCase().replace(/-/g,'_'));
    const redirect_url = '/inspector-protocol/network/resources/echo-headers.php?headers='
        + headersToDump.join(':');
    return fetch('/inspector-protocol/fetch/resources/redirect.pl?' + redirect_url, {headers}).then(r => r.text());
  })()`);

  const beforeRequest = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({requestId: beforeRequest.requestId});
  const beforeRedirect = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({
    requestId: beforeRedirect.requestId,
    headers: [
      {name: "X-DevToolsTest2", value: ""},
      {name: "X-DevToolsTest3", value: "replaced with new value"},
      {name: "X-DevToolsTest4", value: "added"},
    ]
  });
  testRunner.log('Request headers after redirect');
  testRunner.log(await bodyPromise);

  testRunner.completeTest();
})

