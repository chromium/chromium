(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      'Tests emulation of User-Agent header string when the request is redirected.');

  await dp.Page.enable();
  await dp.Network.enable();
  await dp.Emulation.setUserAgentOverride({userAgent: 'ua-set-by-devtools', acceptLanguage: 'ko, en'});

  // redirect.php redirects to /inspector-protocol/emulation/resources/echo-headers.php.
  const redirectUrl = testRunner.url('resources/redirect.php');

  // Navigate to redirect.php.
  testRunner.log("Navigate to redirect.php");
  dp.Page.navigate({url: redirectUrl});
  const navigationResponseReceived = await dp.Network.onceResponseReceived();
  await dp.Network.onceLoadingFinished();
  var navigationResponse = (await dp.Network.getResponseBody({requestId: navigationResponseReceived.params.requestId}));
  printHeader(navigationResponse.result.body, 'User-Agent');
  printHeader(navigationResponse.result.body, 'Accept-Language');

  // Use the fetch() API.
  testRunner.log("Fetch redirect.php");
  const fetchResponseBody = await session.evaluateAsync(`fetch("${redirectUrl}").then(r => r.text())`);
  printHeader(fetchResponseBody, 'User-Agent');
  printHeader(fetchResponseBody, 'Accept-Language');

  // Use an XHR request.
  testRunner.log("XHR redirect.php");
  dp.Runtime.evaluate({expression: `
    {
      let xhr = new XMLHttpRequest();
      xhr.open('GET', '${redirectUrl}');
      xhr.send();
    }
  `});
  const xhrResponse = await dp.Network.getResponseBody({requestId: (await dp.Network.onceResponseReceived()).params.requestId});
  printHeader(xhrResponse.result.body, 'User-Agent');
  printHeader(xhrResponse.result.body, 'Accept-Language');

  function printHeader(response_body, name) {
    for (const header of response_body.split('\n')) {
      if (header.startsWith(name))
        testRunner.log(header);
    }
  }

  testRunner.completeTest();
})
