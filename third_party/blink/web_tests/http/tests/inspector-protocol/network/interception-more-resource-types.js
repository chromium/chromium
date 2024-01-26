(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that ping and CSP violations resource types are correctly identified by Network.requestIntercepted`);

  dp.Network.enable();
  dp.Page.enable();
  dp.Runtime.enable();

  dp.Network.setRequestInterception({patterns: [{}]});
  session.evaluate(`
      navigator.sendBeacon('beacon','this is Major Tom to ground control');
  `);
  const params = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted URL: ${params.request.url} type: ${params.resourceType}`);
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId});

  dp.Page.navigate({url: 'http://127.0.0.1:8000/security/contentSecurityPolicy/resources/generate-csp-report.php'});
  for (;;) {
    const params = (await dp.Network.onceRequestIntercepted()).params;
    dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId});
    if (/save-report.php/.test(params.request.url)) {
      testRunner.log(`Intercepted URL: ${params.request.url} type: ${params.resourceType}`);
      break;
    }
  }

  testRunner.completeTest();
})
