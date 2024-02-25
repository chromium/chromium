(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that navigation request resulting in HTTP 204 is reported as failed.`);

  dp.Network.enable();
  dp.Page.enable();
  dp.Network.onResponseReceived(event => {
    const response = event.params.response;
    testRunner.log(`responseReceived: ${response.url} ${response.status} ${response.statusText}`);
  });
  dp.Network.onLoadingFailed(event => {
    const params = event.params;
    testRunner.log(`loadingFailed: canceled = ${params.canceled}`);
    testRunner.completeTest();
  });
  page.navigate('http://127.0.0.1:8000/inspector-protocol/page/resources/http204.php');
})
