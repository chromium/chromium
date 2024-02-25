(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests test response bodies are evicted in accordance with configured max retention sizes.`);

  await dp.Network.enable({maxTotalBufferSize: 300, maxResourceBufferSize: 200});

  const resourceUrl = '/devtools/network/resources/resource.php';
  const requests = [];
  dp.Network.onRequestWillBeSent(event => {requests.push({
      requestId: event.params.requestId,
      url: event.params.request.url});
  });
  await session.evaluateAsync(`fetch("${resourceUrl}?size=200").then(r => r.text())`);
  await session.evaluateAsync(`fetch("${resourceUrl}?size=100").then(r => r.text())`);
  await session.evaluateAsync(`fetch("${resourceUrl}?size=201").then(r => r.text())`);
  await session.evaluateAsync(`fetch("${resourceUrl}?size=100").then(r => r.text())`);

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
  for (let i = 0; i < requests.length; ++i)
    await getResponseBodyAndDump(i);

  testRunner.completeTest();
})
