(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that navigation instrumentation doesn't fail with a long async stack chain.`);

  dp.Debugger.enable();
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  dp.Network.enable();
  session.evaluateAsync(`
    var f = function() {
      var iframe = document.createElement('iframe');
      iframe.src = '${testRunner.url('./resources/simple-iframe.html?')}';
      document.body.appendChild(iframe);
    }
    for (var i = 0; i < 32; ++i) {
      var fn = eval('(function(c) { Promise.resolve().then(c); })');
      f = fn.bind(null, f);
    }
    f();
  `);
  const params = (await dp.Network.onceRequestWillBeSent()).params;
  testRunner.log(`URL: ${params.request.url}`);
  testRunner.log('Initiator: ');
  testRunner.log(params.initiator);
  testRunner.completeTest();
})
