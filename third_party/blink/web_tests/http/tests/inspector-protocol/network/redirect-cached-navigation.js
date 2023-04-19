(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
    `Verifies that redirects to a cached URL do not produce extra info events\n`);

  function printRequest(request) {
    testRunner.log(`request:`);
    testRunner.log(`  url: ${request.params.request.url}`);
    testRunner.log(`  !!redirectResponse: ${!!request.params.redirectResponse}`);
    testRunner.log(`  redirectHasExtraInfo: ${request.params.redirectHasExtraInfo}`);
  }
  function printResponse(response) {
    testRunner.log(`response:`);
    testRunner.log(`  fromDiskCache: ${response.params.response.fromDiskCache}`);
    testRunner.log(`  hasExtraInfo: ${response.params.hasExtraInfo}`);
  }

  dp.Network.onRequestWillBeSent(printRequest);
  dp.Network.onResponseReceived(printResponse);

  let requestExtraInfoCount = 0;
  let responseExtraInfoCount = 0;
  dp.Network.onRequestWillBeSentExtraInfo(() => requestExtraInfoCount++);
  dp.Network.onResponseReceivedExtraInfo(() => responseExtraInfoCount++);

  dp.Network.enable();
  dp.Page.enable();
  testRunner.log('Fresh redirect:');
  dp.Page.navigate({url: testRunner.url('resources/redirect-cached.php')});
  await dp.Network.onceLoadingFinished();
  testRunner.log('  requestExtraInfoCount: ' + requestExtraInfoCount);
  testRunner.log('  responseExtraInfoCount: ' + responseExtraInfoCount);
  responseExtraInfoCount = requestExtraInfoCount = 0;

  testRunner.log('\nRedirect to a cached resource:');
  dp.Page.navigate({url: testRunner.url('resources/redirect-cached.php')});
  await dp.Network.onceLoadingFinished();
  testRunner.log('  requestExtraInfoCount: ' + requestExtraInfoCount);
  testRunner.log('  responseExtraInfoCount: ' + responseExtraInfoCount);
  testRunner.completeTest();
});
