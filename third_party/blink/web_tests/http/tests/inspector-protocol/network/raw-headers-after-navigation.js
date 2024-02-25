(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
    `Tests that raw response headers are correctly reported after navigation.\n`
  );

  async function fetchAndLogResponseStatus() {
    session.evaluate(`fetch('http://devtools.test:8000/devtools/network/resources/resource.php?cached=1')`);
    const [responseReceived, responseReceivedExtraInfo] = await Promise.all([
      dp.Network.onceResponseReceived(),
      dp.Network.onceResponseReceivedExtraInfo()]);
    testRunner.log('responseRecevied status: ' + responseReceived.params.response.status);
    testRunner.log('responseReceviedExtraInfo status: ' + responseReceivedExtraInfo.params.statusCode);
  }

  await dp.Page.enable();
  await dp.Network.enable();
  await dp.Page.navigate({url: 'http://devtools.test:8000/inspector-protocol/network/resources/hello-world.html'});

  testRunner.log('Fetching resource:');
  await fetchAndLogResponseStatus();

  testRunner.log('Fetching same resource again:');
  await fetchAndLogResponseStatus();

  testRunner.completeTest();
})
