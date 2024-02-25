(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/test-page.html',
      'Tests that Referer header can be overriden even when it would violate referrer policy');

  dp.Network.enable();
  await dp.Network.setExtraHTTPHeaders({headers: {'ReFeReR': 'https://127.0.0.1:8000/'}});
  session.evaluate(`fetch('${testRunner.url('./resources/echo-headers.php?headers=HTTP_REFERER')}')`);

  var response = (await dp.Network.onceLoadingFinished()).params;
  var content = await dp.Network.getResponseBody({requestId: response.requestId});
  testRunner.log(`Referer header: ${content.result.body}`);
  testRunner.completeTest();
})
