(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests cached resource content is discarded when cached is disabled if content size is too big for the network agent's storage`);

  const resourceUrl = '/devtools/network/resources/resource.php';
  const requests = [];
  await dp.Network.enable({maxTotalBufferSize: 100, maxResourceBufferSize: 100});
  dp.Network.onRequestWillBeSent(event => {requests.push({
      requestId: event.params.requestId,
      url: event.params.request.url});
  });
  await session.evaluateAsync(`
      window.image = document.createElement("img");
      new Promise(resolve => {
        image.src = "${resourceUrl}?size=400&type=image&random=1";
        image.onload = resolve;
        document.body.appendChild(image);
      });
  `);

  async function getResponseBodyAndDump(index) {
    const requestId = requests[index].requestId;
    const response = await dp.Network.getResponseBody({requestId});
    let result;
    if (response.error) {
      result = response.error.message;
    } else {
      const body = response.result.base64Encoded ? atob(response.result.body) : response.result.body;
      result = `body size: ${body.length}`;
    }
    testRunner.log(`#${index}: ${requests[index].url}, response body: ${result}`);
  }
  testRunner.log('Requesting response body with cache enabled')
  await getResponseBodyAndDump(0);
  await session.evaluateAsync(`
      new Promise(resolve => {
        image.src = "${resourceUrl}?size=400&type=image&random=1";
        image.onload = resolve;
      });
  `);
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  for (var i = 0; i < 3; ++i) {
    await session.evaluateAsync(`new Promise(resolve => GCController.asyncCollectAll(resolve))`);
  }
  testRunner.log('Requesting response body with cache disabled')
  await getResponseBodyAndDump(0);
  testRunner.completeTest();
})
