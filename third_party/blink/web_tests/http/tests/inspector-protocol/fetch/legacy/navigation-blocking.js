(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that top-frame navigations are correctly blocked when intercepted.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Fetch.enable({patterns: [{}]});

  dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/meta-tag.html'
  });
  const frame1 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`intercepted: ${frame1.request.url}, continuing`);
  dp.Fetch.continueRequest({requestId: frame1.requestId});

  const frame2 = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`intercepted: ${frame2.request.url}, cancelling`);
  dp.Fetch.failRequest({requestId: frame2.requestId, errorReason: 'Aborted'});

  const location = await session.evaluate('location.href');

  testRunner.log(`location.href = ${location}`);

  testRunner.completeTest();
})
