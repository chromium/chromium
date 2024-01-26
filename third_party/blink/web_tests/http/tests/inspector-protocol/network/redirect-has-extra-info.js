(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
    `Verifies that requestWillBeSent.redirectEmittedExtraInfo matches the presence of ExtraInfo events on redirects.\n`);

  await dp.Network.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Page.enable();

  await dp.Network.setRequestInterception({patterns: [{}]});
  dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<iframe src="http://127.0.1.1:8000/whatever"></iframe>';
  `});

  function printRequest(request) {
    testRunner.log(`request:`);
    testRunner.log(`  url: ${request.params.request.url}`);
    testRunner.log(`  !!redirectResponse: ${!!request.params.redirectResponse}`);
    testRunner.log(`  redirectHasExtraInfo: ${request.params.redirectHasExtraInfo}`);
  }
  function printResponse(response) {
    testRunner.log(`response:`);
    testRunner.log(`  hasExtraInfo: ${response.params.hasExtraInfo}`);
  }

  dp.Network.onRequestWillBeSent(printRequest);

  let requestExtraInfoCount = 0;
  let responseExtraInfoCount = 0;
  dp.Network.onRequestWillBeSentExtraInfo(() => requestExtraInfoCount++);
  dp.Network.onResponseReceivedExtraInfo(() => responseExtraInfoCount++);

  let params = (await dp.Network.onceRequestIntercepted()).params;
  const response = "HTTP/1.1 303 See other\r\n" +
      "Location: http://127.0.0.1:8000/devtools/resources/empty.html\r\n\r\n";
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId, rawResponse: btoa(response)});
  params = (await dp.Network.onceRequestIntercepted()).params;
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId});

  printResponse(await dp.Network.onceResponseReceived());

  if (!responseExtraInfoCount)
    await dp.Network.onceResponseReceivedExtraInfo();
  testRunner.log(`request ExtraInfo count: ${requestExtraInfoCount}`);
  testRunner.log(`response ExtraInfo count: ${responseExtraInfoCount}`);

  testRunner.completeTest();
});
