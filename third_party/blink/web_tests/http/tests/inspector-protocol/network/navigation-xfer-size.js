(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that transfer size is correctly reported for navigations.`);

  dp.Network.enable();
  session.evaluateAsync(`
    var iframe = document.createElement('iframe');
    iframe.src = '/inspector/network/resources/resource.php?gzip=1&size=8000';
    document.body.appendChild(iframe);
  `);
  let [response, responseExtraInfo] = await Promise.all([
    dp.Network.onceResponseReceived(),
    dp.Network.onceResponseReceivedExtraInfo()]);
  response = response.params.response;

  const encodedLength = (await dp.Network.onceLoadingFinished()).params.encodedDataLength;
  if (encodedLength > 2000)
    testRunner.log(`FAIL: encoded data length is suspiciously large (${encodedLength})`);

  const headersLength = responseExtraInfo.params.headersText.length;
  const contentLength = +response.headers['Content-Length'];
  if (headersLength + contentLength !== encodedLength)
    testRunner.log(`FAIL: headersLength (${headersLength}) + contentLength (${contentLength}) !== encodedLength (${encodedLength})`)
  testRunner.completeTest();
})
