(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that browser-initiated navigation honors Network.setCacheDisabled\n`);

  async function navigateAndGetResponse() {
    dp.Page.navigate({url: testRunner.url('resources/cached.php')});
    const [responseReceived, responseReceivedExtraInfo] = await Promise.all([
      dp.Network.onceResponseReceived(),
      dp.Network.onceResponseReceivedExtraInfo()]);
    var response = responseReceived.params;
    await dp.Network.onceLoadingFinished();
    var content = (await dp.Network.getResponseBody({requestId: response.requestId})).result.body;
    if (typeof content != 'string' || !content.startsWith('<html>'))
      testRunner.fail(`Invalid response: ${content}`);
    return {
      status: response.response.status,
      extraInfoStatus: responseReceivedExtraInfo.params.statusCode,
      content: content
    };
  }
  await dp.Page.enable();
  await dp.Network.enable();
  let response = await navigateAndGetResponse();
  testRunner.log(`Original navigation, should not be cached:`);
  testRunner.log(`  responseReceived status: ${response.status}`);
  testRunner.log(`  responseReceivedExtraInfo status: ${response.extraInfoStatus}\n`);

  let response2 = await navigateAndGetResponse();
  testRunner.log(`Second navigation, should be cached:`);
  testRunner.log(`  responseReceived status: ${response2.status}`);
  testRunner.log(`  responseReceivedExtraInfo status: ${response2.extraInfoStatus}`);
  testRunner.log(`  cached: ${response.content === response2.content}\n`);

  await dp.Network.setCacheDisabled({cacheDisabled: true});
  let response3 = await navigateAndGetResponse();

  testRunner.log(`Navigation with cache disabled:`);
  testRunner.log(`  responseReceived status: ${response3.status}`);
  testRunner.log(`  responseReceivedExtraInfo status: ${response3.extraInfoStatus}`);
  testRunner.log(`  cached: ${response3.content === response2.content}`);
  testRunner.completeTest();
})
