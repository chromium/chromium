(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests providing HTTP auth credentials for main resource over DevTools protocol.`);

  await session.evaluateAsync(`
    var frame = document.createElement('iframe');
    document.body.appendChild(frame);
  `);

  await dp.Network.enable();
  await dp.Network.setRequestInterception({patterns: [{}]});
  dp.Network.onRequestIntercepted(event =>
    dp.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId}));
  await dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/network/resources/simple-iframe.html'});
  testRunner.completeTest();
})
