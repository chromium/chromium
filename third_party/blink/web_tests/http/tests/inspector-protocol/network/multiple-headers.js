 (async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that multiple HTTP headers with same name are correctly folded into one LF-separated line.`);

  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) =>
      testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => {
    testRunner.die('Timeout', errorForLog);
  }, 5000);

  const url = 'http://127.0.0.1:8000/inspector-protocol/network/resources/multiple-headers.php';
  await dp.Network.enable();
  await dp.Page.enable();

  session.evaluate(`fetch("${url}?fetch=1").then(r => r.text())`);
  errorForLog = new Error();
  const [fetchResponse, fetchResponseExtraInfo] = await Promise.all([
    dp.Network.onceResponseReceived(),
    dp.Network.onceResponseReceivedExtraInfo()]);
  testRunner.log(`Pragma header of fetch of ${fetchResponse.params.response.url}:`);
  testRunner.log(`Network.responseReceived: ${fetchResponse.params.response.headers['Access-Control-Pragma']}`);
  testRunner.log(`Network.responseReceivedExtraInfo: ${fetchResponseExtraInfo.params.headers['Access-Control-Pragma']}`);
  testRunner.log('');
  errorForLog = new Error();
  await dp.Network.onceLoadingFinished();

  session.evaluate(`
    var f = document.createElement('frame');
    f.src = "${url}";
    document.body.appendChild(f);
  `);
  errorForLog = new Error();
  const [navigationResponse, navigationResponseExtraInfo] = await Promise.all([
    dp.Network.onceResponseReceived(),
    dp.Network.onceResponseReceivedExtraInfo()]);
  testRunner.log(`Pragma header of navigation to ${navigationResponse.params.response.url}:`);
  testRunner.log(`Network.responseReceived: ${navigationResponse.params.response.headers['Access-Control-Pragma']}`);
  testRunner.log(`Network.responseReceivedExtraInfo: ${navigationResponseExtraInfo.params.headers['Access-Control-Pragma']}`);
  testRunner.completeTest();
})
