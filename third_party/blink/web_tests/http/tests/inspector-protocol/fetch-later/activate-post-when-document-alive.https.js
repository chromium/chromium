(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {_, session, dp} = await testRunner.startBlank(
      `Tests that appropriate Network domain events are dispatched for a fetchLater POST request when its Document is alive.`);

  dp.Page.enable();
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();

  const gatherNetworkEvents =
      await testRunner.loadScript('resources/gather-network-events.js');
  const events = gatherNetworkEvents(testRunner, dp, {requests: 1, log: true});

  await session.evaluateAsync(async () => {
    const url = '/devtools/network/resources/resource.php';

    function waitForActivated(fetchLaterResult) {
      function waitFor(result) {
        if (result && result.activated) {
          return result;
        }
        return new Promise(resolve => setTimeout(resolve, 100))
            .then(() => waitFor(fetchLaterResult));
      }
      return waitFor();
    }
    return waitForActivated(fetchLater(url, {
      activateAfter: 0,
      method: 'POST',
      body: JSON.stringify({foo: 'bar'})
    }));
  });

  const requestId = (await events)
                        .find(event => event.params.type === 'Fetch')
                        ?.params.requestId;
  const msg = await dp.Network.getResponseBody({requestId: requestId});
  if (msg.error) {
    testRunner.log(msg.error, 'Unable to get fetchLater response body');
  } else {
    testRunner.log(msg.result, 'fetchLater response body');
  }

  testRunner.completeTest();
})
