(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that ping and CSP violations resource types are correctly identified by Network.requestIntercepted`);

  dp.Network.enable();
  dp.Page.enable();
  dp.Runtime.enable();

  dp.Fetch.enable({patterns: [{}]});
  session.evaluate(`
      navigator.sendBeacon('beacon','this is Major Tom to ground control');
  `);
  const params = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(
      `Intercepted URL: ${params.request.url} type: ${params.resourceType}`);
  dp.Fetch.continueRequest({requestId: params.requestId});

  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/security/contentSecurityPolicy/resources/generate-csp-report.php'
  });
  for (;;) {
    const params = (await dp.Fetch.onceRequestPaused()).params;
    dp.Fetch.continueRequest({requestId: params.requestId});
    if (/save-report.php/.test(params.request.url)) {
      testRunner.log(`Intercepted URL: ${params.request.url} type: ${
          params.resourceType}`);
      break;
    }
  }

  testRunner.completeTest();
})
