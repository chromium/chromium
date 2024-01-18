(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception to ensure patterns ignore ref hashes when they pattern match.`);

  await session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  await session.protocol.Page.enable();
  testRunner.log('Page agent enabled');

  session.protocol.Network.onRequestIntercepted(async event => {
    var filename = event.params.request.url.split('/').pop();
    testRunner.log('Request Intercepted: ' + filename);
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId, errorReason: 'AddressUnreachable'});
    testRunner.completeTest();
  });

  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*/image.png"}]});
  session.evaluate(`
    var img = new Image();
    img.src = 'image.png#SOME_HASH';
    document.body.appendChild(img);
  `);

  await session.protocol.Page.onceFrameStoppedLoading(async event => {
    testRunner.log('Page.FrameStoppedLoading\n');
  });
})
