(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception of specified resource types.`);

  await session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  await session.protocol.Page.enable();
  testRunner.log('Page agent enabled');

  session.protocol.Network.onRequestIntercepted(async event => {
    var filename = event.params.request.url.split('/').pop();
    testRunner.log('Request Intercepted: ' + filename);
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId, errorReason: 'AddressUnreachable'});
  });

  testRunner.log('Intercept scripts only');
  await session.protocol.Network.setRequestInterception({patterns: [{resourceType: "Script"}]});
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/resource-iframe.html')}';
    document.body.appendChild(iframe);
  `);

  await session.protocol.Page.onceFrameStoppedLoading(async event => {
    testRunner.log('Page.FrameStoppedLoading\n');
  });

  testRunner.log('Intercept stylesheets only');
  await session.protocol.Network.setRequestInterception({patterns: [{resourceType: "Stylesheet"}]});
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/resource-iframe.html')}';
    document.body.appendChild(iframe);
  `);

  await session.protocol.Page.onceFrameStoppedLoading(async event => {
    testRunner.log('Page.FrameStoppedLoading\n');
  });

  testRunner.completeTest();
})
