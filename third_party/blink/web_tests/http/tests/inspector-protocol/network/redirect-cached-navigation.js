(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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

  function waitForExtraInfoCounts(count) {
    let requestExtraInfoCount = 0;
    let responseExtraInfoCount = 0;
    return new Promise(resolve => {
      function check() {
        if (requestExtraInfoCount === count && responseExtraInfoCount === count) {
          dp.Network.offRequestWillBeSentExtraInfo(onRequestWillBeSentExtraInfo);
          dp.Network.offResponseReceivedExtraInfo(onResponseReceivedExtraInfo);
          resolve([requestExtraInfoCount, responseExtraInfoCount]);
        }
      }
      function onRequestWillBeSentExtraInfo() {
        requestExtraInfoCount++;
        check();
      }
      function onResponseReceivedExtraInfo() {
        responseExtraInfoCount++;
        check();
      }
      dp.Network.onRequestWillBeSentExtraInfo(onRequestWillBeSentExtraInfo);
      dp.Network.onResponseReceivedExtraInfo(onResponseReceivedExtraInfo);
    });

  }

  dp.Network.enable();
  dp.Page.enable();
  {
    testRunner.log('Fresh redirect:');
    const freshRedirectCounts = waitForExtraInfoCounts(2);
    dp.Page.navigate({url: testRunner.url('resources/redirect-cached.php')});
    await dp.Network.onceLoadingFinished();
    const [requestExtraInfoCount, responseExtraInfoCount] = await freshRedirectCounts;
    testRunner.log('  requestExtraInfoCount: ' + requestExtraInfoCount);
    testRunner.log('  responseExtraInfoCount: ' + responseExtraInfoCount);
  }

  {
    testRunner.log('\nRedirect to a cached resource:');
    const cachedRedirectCounts = waitForExtraInfoCounts(1);
    dp.Page.navigate({url: testRunner.url('resources/redirect-cached.php')});
    await dp.Network.onceLoadingFinished();
    const [requestExtraInfoCount, responseExtraInfoCount] = await cachedRedirectCounts;
    testRunner.log('  requestExtraInfoCount: ' + requestExtraInfoCount);
    testRunner.log('  responseExtraInfoCount: ' + responseExtraInfoCount);
  }

  testRunner.completeTest();
});
