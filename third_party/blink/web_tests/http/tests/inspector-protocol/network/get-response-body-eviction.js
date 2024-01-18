(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests test response bodies are evicted in accordance with configured max retention sizes.`);

  await dp.Network.enable();

  const resourceUrl = '/devtools/network/resources/resource.php';
  const requests = [];
  dp.Network.onRequestWillBeSent(event => {requests.push({
      requestId: event.params.requestId,
      url: event.params.request.url});
  });
  await session.evaluateAsync(`fetch("${resourceUrl}?size=1024").then(r => r.text())`);
  await session.evaluateAsync(`fetch("${resourceUrl}?size=2048").then(r => r.text())`);
  await session.evaluateAsync(`fetch("${resourceUrl}?size=4096").then(r => r.text())`);

  async function getResponseBodyAndDump(index) {
    const requestId = requests[index].requestId;
    const response = await dp.Network.getResponseBody({requestId});
    let result;
    if (response.error) {
      result = response.error.message;
    } else {
      const body = response.result.body;
      result = `body size: ${body.length}`;
    }
    testRunner.log(`#${index}: ${requests[index].url}, response body: ${result}`);
  }
  async function dumpBodies(message) {
    testRunner.log(message);
    for (let i = 0; i < requests.length; ++i)
      await getResponseBodyAndDump(i);
  }

  await dumpBodies(`Before further Network.enable commands`);

  await dp.Network.enable();
  await dumpBodies(`After duplicate Network.enable with default buffer sizes`);

  await dp.Network.enable({maxTotalBufferSize: 3000, maxResourceBufferSize: 3000});
  await dumpBodies(`After max resource size update`);

  dp.Network.disable();
  await dp.Network.enable();
  await dumpBodies(`After Network.disable and Network.enable`);

  testRunner.completeTest();
})
