(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      'Verifies resource type returned by the protocol when a fetch() or XHR is redirected');

  let pathsToFetch = [
    '/inspector-protocol/resources/redirect1.php',
    '/inspector-protocol/resources/redirect2.php',
    '/inspector-protocol/resources/final.php',
  ];

  await dp.Network.enable();

  function logResponse(event) {
    testRunner.log('Response Received');
    testRunner.log(`  url: ${event.params.response.url}`);
    testRunner.log(`  type: ${event.params.type}`);
  }

  for (const path of pathsToFetch) {
    testRunner.log('');
    testRunner.log(`Fetching ${path}`);
    dp.Runtime.evaluate({expression: `fetch('${path}')`});
    logResponse(await dp.Network.onceResponseReceived());

    testRunner.log('');
    testRunner.log(`Making XHR for ${path}`);
    dp.Runtime.evaluate({expression: `
      {
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '${path}');
        xhr.send();
      }
    `});
    logResponse(await dp.Network.onceResponseReceived());
  }

  testRunner.completeTest();
})
