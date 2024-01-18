(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
    'Verifies that ExtraInfo events are emitted for each redirect in a chain in subsequent requests.\n');

  // Clear the cache to prevent interactions with other tests that were running
  // on the same content shell.
  await dp.Network.clearBrowserCache();
  await dp.Network.enable();

  const requests = new Map();
  function pushEvent(name, event) {
    if (!requests.has(event.params.requestId)) {
      requests.set(event.params.requestId, {});
    }
    const request = requests.get(event.params.requestId);
    if (!request[name]) {
      request[name] = [];
    }
    request[name].push(event);
  }

  dp.Network.onRequestWillBeSent(event => {
    pushEvent('requestWillBeSent', event);
  });

  const responseReceivedsPromise = new Promise(resolve => {
    let responseReceivedCount = 0;
    dp.Network.onResponseReceived(event => {
      pushEvent('responseReceived', event);
      responseReceivedCount++;
      if (responseReceivedCount === 2)
        resolve();
    });
  });

  dp.Network.onRequestWillBeSentExtraInfo(event => {
    pushEvent('requestWillBeSentExtraInfo', event);
  });

  const extraInfosPromise = new Promise(resolve => {
    let extraInfoCount = 0;
    dp.Network.onResponseReceivedExtraInfo(event => {
      pushEvent('responseReceivedExtraInfo', event);
      extraInfoCount++;
      if (extraInfoCount === 3)
        resolve();
    });
  });


  const path = '/inspector-protocol/resources/redirect2.php';
  session.evaluate(`
    {
      let xhr = new XMLHttpRequest();
      xhr.open('GET', '${path}');
      xhr.send();
    }
  `);
  session.evaluate(`
    {
      let xhr = new XMLHttpRequest();
      xhr.open('GET', '${path}');
      xhr.send();
    }
  `);

  await responseReceivedsPromise;
  await extraInfosPromise;

  for (const [requestId, request] of requests) {
    const requestWillBeSents = request.requestWillBeSent;
    if (requestWillBeSents) {
      testRunner.log(`requestWillBeSents: ${requestWillBeSents.length}`);
      for (let i = 0; i < requestWillBeSents.length; i++) {
        const requestWillBeSent = requestWillBeSents[i];
        testRunner.log(`  url: ${requestWillBeSent.params.request.url}`);
        testRunner.log(`  redirectHasExtraInfo: ${requestWillBeSent.params.redirectHasExtraInfo}`);
      }
    } else {
      testRunner.log(`requestWilBeSents: none`);
    }

    const responseReceiveds = request.responseReceived;
    if (responseReceiveds) {
      testRunner.log(`responseReceiveds: ${responseReceiveds.length}`);
      for (let i = 0; i < responseReceiveds.length; i++) {
        const responseReceived = responseReceiveds[i];
        testRunner.log(`  url: ${responseReceived.params.response.url}`);
        testRunner.log(`  hasExtraInfo: ${responseReceived.params.hasExtraInfo}`);
      }
    } else {
      testRunner.log(`responseReceiveds: none`);
    }

    const requestWillBeSentExtraInfos = request.requestWillBeSentExtraInfo;
    if (requestWillBeSentExtraInfos) {
      testRunner.log(`requestWillBeSentExtraInfos: ${requestWillBeSentExtraInfos.length}`);
      for (let i = 0; i < requestWillBeSentExtraInfos.length; i++) {
        const requestWillBeSentExtraInfo = requestWillBeSentExtraInfos[i];
        testRunner.log(`  has headers: ${Object.keys(requestWillBeSentExtraInfo.params.headers).length > 0}`);
      }
    }

    const responseReceivedExtraInfos = request.responseReceivedExtraInfo;
    if (responseReceivedExtraInfos) {
      testRunner.log(`responseReceivedExtraInfos: ${responseReceivedExtraInfos.length}`);
      for (let i = 0; i < responseReceivedExtraInfos.length; i++) {
        const responseReceivedExtraInfo = responseReceivedExtraInfos[i];
        testRunner.log(`  has headers: ${Object.keys(responseReceivedExtraInfo.params.headers).length > 0}`);
      }
    }

    testRunner.log('');
  }

  testRunner.completeTest();
})
