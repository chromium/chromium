(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/simple.html',
      `Tests that interceptor has access to the Cookie header.`);

  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Fetch.enable({patterns: [{}]});
  dp.Fetch.onRequestPaused(e => {
    testRunner.log('Cookie: ' + e.params.request.headers['Cookie']);
    dp.Fetch.continueRequest({requestId: e.params.requestId});
  });

  await session.evaluateAsync(`document.cookie = 'foo=bar';`);

  dp.Page.reload();
  await dp.Network.onceLoadingFinished();

  testRunner.completeTest();
})
