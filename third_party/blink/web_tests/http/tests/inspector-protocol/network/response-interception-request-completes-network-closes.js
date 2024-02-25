(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests to ensure that a request completes if agent disconnects and no response given.`);

  session.protocol.Network.onRequestIntercepted(async event => {
      testRunner.log('Request Intercepted: ' + event.params.request.url.split('/').pop());
      testRunner.log('Disabling Network Agent');
      session.protocol.Network.disable();
      testRunner.log('');
  });

  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*", interceptionStage: 'HeadersReceived'}]});

  var responseContent = await session.evaluateAsync(`fetch('/devtools/network/resources/resource.php?size=100').then(response => response.text())`);
  testRunner.log('Body: ');
  testRunner.log(responseContent);

  testRunner.completeTest();
})
