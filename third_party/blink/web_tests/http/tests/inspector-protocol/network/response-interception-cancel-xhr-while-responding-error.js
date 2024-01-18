(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests to ensure error message on request cancelled while intercepting response.`);

  session.protocol.Network.onRequestIntercepted(async event => {
    testRunner.log('Request Intercepted: ' + event.params.request.url.split('/').pop());

    testRunner.log('Aborting request');
    session.evaluate(`window.xhr.abort();`);
    await new Promise(resolve => session.protocol.Network.onLoadingFailed(resolve));

    testRunner.log('Renderer received abort signal');
    testRunner.log('Continuing intercepted request');
    var result = await session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
    testRunner.log('Error message: ' + result.error.message);
    testRunner.log('');
    testRunner.completeTest();
  });

  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*", interceptionStage: 'HeadersReceived'}]});

  await new Promise(resolve => {
    session.protocol.Network.onResponseReceived(resolve);
    session.evaluate(`
      window.xhr = new XMLHttpRequest();
      // This script will send headers then wait 10 seconds before sending body.
      window.xhr.open('GET', '/devtools/network/resources/resource.php?send=10000&nosniff=1', true);
      window.xhr.send();
    `);
  });
})
