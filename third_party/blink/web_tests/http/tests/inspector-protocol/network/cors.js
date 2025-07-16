(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure CORS with preflight requests are correctly reported.`);

  // Skip some fields that are not important for this test.
  const stabilizeNames = [
    'headers', 'wallTime', 'response', 'stack', ...TestRunner.stabilizeNames
  ];
  // This fields are important for this test, but are not stable, so they have
  // to stabilized before logging.
  const stabilizeValues = ['frameId', 'requestId'];

  function isPreflight(event) {
    return event.params?.initiator?.type === 'preflight' ||
        event.params?.type === 'Preflight';
  }

  // This url should be cross origin.
  const url =
      `https://127.0.0.1:8443/inspector-protocol/network/resources/cors-headers.php`;

  await dp.Network.enable();

  const events = [];

  dp.Network.onLoadingFailed(event => events.push(event));
  dp.Network.onRequestWillBeSent(event => events.push(event));
  dp.Network.onResponseReceived(event => events.push(event));

  testRunner.log('Creating request from the main frame...');
  // Script creating a non-trivial CORS request.
  const scriptContent = `fetch('${url}?origin=*', { method: 'PUT' })`;

  // Run the script in the main frame.
  await session.evaluateAsync(scriptContent);

  testRunner.log('Creating request from the iframe...');
  await session.evaluateAsync(`
    (async ()=> {
      const iframe = document.body.appendChild(document.createElement('iframe'));
      iframe.contentDocument.write(
        "<script>window.scriptResult = ${scriptContent}<\/script>");
      return await iframe.contentWindow.scriptResult;
    })()
  `);

  // The preflight and the main requests are racy, so they are logged
  // separately for expectation consistency.
  testRunner.log(
      events.filter(event => isPreflight(event)),
      'Preflight events: ', stabilizeNames, stabilizeValues);
  testRunner.log(
      events.filter(event => !isPreflight(event)),
      'Other events: ', stabilizeNames, stabilizeValues);

  testRunner.completeTest();
})
