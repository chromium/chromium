(async function() {
  TestRunner.addResult(
      `Verifies that the main page request repeated by a service worker appears in the network log.`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadTestModule('network_test_runner');

  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('network');

  await TestRunner.navigatePromise(
      'resources/service-worker-repeat-fetch.html');
  await TestRunner.evaluateInPageAsync('installSWAndWaitForActivated()');
  await new Promise(
      resolve => ApplicationTestRunner.waitForServiceWorker(resolve));
  await TestRunner.reloadPagePromise();
  TestRunner.addResult('');

  const requests = NetworkTestRunner.networkRequests();
  for (const request of requests) {
    const networkManager = SDK.NetworkManager.forRequest(request);
    TestRunner.addResult('request.url(): ' + request.url());
    TestRunner.addResult(
        'request.target.type(): ' + networkManager ?
            networkManager.target().type() :
            'no network manager for this request');
    TestRunner.addResult('');
  }

  TestRunner.completeTest();
})();
