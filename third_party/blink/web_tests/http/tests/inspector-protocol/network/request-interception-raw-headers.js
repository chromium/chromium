(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that raw response headers are correctly reported in case of interception.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'simple-iframe.html': event => helper.allowRequest(event),
  };

  await helper.startInterceptionTest(requestInterceptedDict);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/simple-iframe.html')}';
    document.body.appendChild(iframe);
  `);

  await dp.Network.onRequestWillBeSentExtraInfo(event => {
    testRunner.log(`Connection raw header present: ${!!event.params.headers['Connection'].length}`);
  });
})
