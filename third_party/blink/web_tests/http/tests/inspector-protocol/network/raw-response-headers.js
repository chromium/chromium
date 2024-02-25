(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that raw response headers are correctly reported in case of revalidation.`);

  dp.Network.enable();
  session.protocol.Page.reload();
  const [response, responseExtraInfo] = await Promise.all([
    dp.Network.onceResponseReceived(),
    dp.Network.onceResponseReceivedExtraInfo()]);

  testRunner.log(`Response status: ${response.params.response.status} ${response.params.response.statusText}`);
  testRunner.log(`ResponseExtraInfo status: ${responseExtraInfo.params.statusCode}`);
  const headersText = responseExtraInfo.params.headersText;
  testRunner.log(`ResponseExtraInfo status line: ${headersText.substring(0, headersText.indexOf('\r'))}`);
  testRunner.log(`Response headers text present: ${!!headersText}`);
  testRunner.log(`Content-Length present in blink headers: ${Object.keys(response.params.response.headers).indexOf('Content-Length') >= 0}`);
  testRunner.log(`Content-Length present in ExtraInfo headers: ${Object.keys(responseExtraInfo.params.headers).indexOf('Content-Length') >= 0}`);

  testRunner.completeTest();
})
