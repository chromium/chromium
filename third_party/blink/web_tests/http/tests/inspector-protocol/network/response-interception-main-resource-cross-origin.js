(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests to ensure response interception works with cross origin redirects.`);
  dp.Network.onRequestIntercepted(event => {
      testRunner.log('Request Intercepted: ' + event.params.request.url);
      testRunner.log('');
      testRunner.completeTest();
  });

  await dp.Network.clearBrowserCookies();
  await dp.Network.clearBrowserCache();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  testRunner.log('Setting interception patterns to intercept: http://127.0.0.1:8000/devtools/network/resources/redirect-cross-origin-empty-html.php');
  await dp.Network.setRequestInterception({patterns: [{urlPattern: "http://localhost:8000/devtools/network/resources/empty.html", interceptionStage: 'HeadersReceived'}]});
  testRunner.log('');

  testRunner.log('Navigating to: http://127.0.0.1:8000/devtools/network/resources/redirect-cross-origin-empty-html.php');
  testRunner.log('--- This url should redirect to: http://localhost:8000/devtools/network/resources/empty.html ---');
  dp.Page.navigate({url: 'http://127.0.0.1:8000/devtools/network/resources/redirect-cross-origin-empty-html.php'});
  testRunner.log('');
})
