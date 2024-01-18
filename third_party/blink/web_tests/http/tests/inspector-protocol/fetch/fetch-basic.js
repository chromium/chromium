(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests basic function of the fetch domain.`);

  var FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  var helper = new FetchHelper(testRunner, testRunner.browserP());
  await helper.enable();
  helper.onceRequest().continueRequest();
  dp.Page.enable();
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();

  helper.onceRequest().fulfill({
    responseCode: 200,
    responseHeaders: [],
    body: btoa("overriden response body")
  });

  const url = '/devtools/network/resources/resource.php';
  let content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after Fetch.fulfillRequest: ${content}`);

  helper.onceRequest().continueRequest();
  content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after fetch.continueRequest: ${content}`);

  helper.onceRequest().continueRequest({url: 'http://127.0.0.1:8000/devtools/network/resources/resource.php?size=42'});
  content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after fetch.continueRequest with new URL: ${content}`);

  helper.onceRequest().fail({errorReason: 'AccessDenied'});
  content = await session.evaluateAsync(`fetch("${url}").then(r => t.text()).catch(() => 'fail')`);
  testRunner.log(`Response after fetch.failRequest: ${content}`);

  testRunner.log(`Sending invalid header (should result in error)`);
  const echoHeaders = '/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_X_DEVTOOLS_TEST';
  session.evaluateAsync(`fetch("${echoHeaders}").then(r => r.text())`);
  await helper.onceRequest().continueRequest({headers: [{name: 'X-DevTools-Test: ', value: 'foo'}]});

  testRunner.completeTest();
})
