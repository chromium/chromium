(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    'https://127.0.0.1:8443/inspector-protocol/resources/empty.html',
    `Tests setting accepted encodings.`);

  await dp.Network.enable();

  const tests = [
    [],
    ['gzip'],
    ['gzip', 'br'],
    ['gzip', 'br', 'deflate'],
    ['br'],
    ['zstd', 'gzip'],
    ['zstd'],
  ];

  async function runTest(injectTestResource) {
    injectTestResource();
    const response = (await dp.Network.onceLoadingFinished()).params;
    const content = await dp.Network.getResponseBody({requestId: response.requestId});
    testRunner.log('Server received Accept-Encoding header: ' + content.result.body);
  }

  async function runTests(injectTestResource) {
    testRunner.log('Testing Accept-Encoding header before overrides are set:');
    await runTest(injectTestResource);
    for (const test of tests) {
      await dp.Network.setAcceptedEncodings({encodings: test});
      testRunner.log('Testing Accept-Encoding header with the override: ' + test.join(','));
      await runTest(injectTestResource);
    }
    await dp.Network.clearAcceptedEncodingsOverride();
    testRunner.log('Testing Accept-Encoding header after clearAcceptedEncodingsOverride');
    await runTest(injectTestResource);
  }

  const resourceURL = 'https://127.0.0.1:8443/inspector-protocol/network/resources/content-encoding.php';
  testRunner.log("Testing that navigation requests have overrides applied:\n");
  await runTests(() => session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${resourceURL}';
    document.body.appendChild(iframe);
  `));
  testRunner.log("Testing that page fetch requests have overrides applied:\n");
  await runTests(() => session.evaluate(`fetch('${resourceURL}')`));

  testRunner.log('Invalid encoding error:');
  testRunner.log(await dp.Network.setAcceptedEncodings({encodings: ['unknown']}));

  testRunner.completeTest();
})
