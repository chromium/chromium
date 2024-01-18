(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests adding extra HTTP headers.`);

  await dp.Network.enable();
  await dp.Network.setExtraHTTPHeaders({headers: {"X-DevTools-Test": "Hello, world!"}});
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/echo-headers.php?headers=HTTP_X_DEVTOOLS_TEST')}';
    document.body.appendChild(iframe);
  `);
  var response = (await dp.Network.onceLoadingFinished()).params;

  var content = await dp.Network.getResponseBody({requestId: response.requestId});
  testRunner.log(content.result.body);
  testRunner.completeTest();
})
