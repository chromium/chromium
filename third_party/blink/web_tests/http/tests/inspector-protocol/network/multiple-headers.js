 (async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that multiple HTTP headers with same name are correctly folded into one LF-separated line.`);

  const url = 'http://127.0.0.1:8000/inspector-protocol/network/resources/multiple-headers.php';
  await dp.Network.enable();
  await dp.Page.enable();

  session.evaluate(`fetch("${url}?fetch=1").then(r => r.text())`);
  const [fetchResponse, fetchResponseExtraInfo] = await Promise.all([
    dp.Network.onceResponseReceived(),
    dp.Network.onceResponseReceivedExtraInfo(),
    dp.Network.onceLoadingFinished()
  ]);
  testRunner.log(`Pragma header of fetch of ${fetchResponse.params.response.url}:`);
  testRunner.log(`Network.responseReceived: ${fetchResponse.params.response.headers['Access-Control-Pragma']}`);
  testRunner.log(`Network.responseReceivedExtraInfo: ${fetchResponseExtraInfo.params.headers['Access-Control-Pragma']}`);
  testRunner.log('');

  session.evaluate(`
    var f = document.createElement('frame');
    f.src = "${url}";
    document.body.appendChild(f);
  `);
  const [navigationResponse, navigationResponseExtraInfo] = await Promise.all([
    dp.Network.onceResponseReceived(),
    dp.Network.onceResponseReceivedExtraInfo()]);
  testRunner.log(`Pragma header of navigation to ${navigationResponse.params.response.url}:`);
  testRunner.log(`Network.responseReceived: ${navigationResponse.params.response.headers['Access-Control-Pragma']}`);
  testRunner.log(`Network.responseReceivedExtraInfo: ${navigationResponseExtraInfo.params.headers['Access-Control-Pragma']}`);
  testRunner.completeTest();
})
