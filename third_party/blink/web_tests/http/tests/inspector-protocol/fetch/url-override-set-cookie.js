(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests handling of Set-Cookie following a request with overridden URL.`);

  const url = 'http://127.0.0.1:8000/protocol/inspector-protocol-page.html';

  dp.Fetch.enable({patterns: [{}, {requestStage: 'Response'}]});
  const navigationPromise = session.navigate(url + '?originalURL');
  const requestId = (await dp.Fetch.onceRequestPaused()).params.requestId;
  dp.Fetch.continueRequest({requestId, url: url + '?modifiedURL'});
  await dp.Fetch.onceRequestPaused();
  dp.Fetch.fulfillRequest({
    requestId,
    responseCode: 200,
    body: btoa('<body>hello world!</body>'),
    headers: [{name: 'Set-Cookie', value: 'name=value'}]
  });

  await navigationPromise;
  testRunner.log(await session.evaluate('location.href'));
  testRunner.completeTest();
})
