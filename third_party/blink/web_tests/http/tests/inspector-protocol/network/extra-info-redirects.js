(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Verifies that Network.*ExtraInfo events get assigned to the correct request hops in cross-origin redirects.\n');

  await dp.Network.clearBrowserCache();
  await dp.Network.enable();

  const requestWillBeSents = [];
  const requestWillBeSentExtraInfos = [];
  const responseReceiveds = [];
  const responseReceivedExtraInfos = [];
  const requestIds = new Set();

  let resolveExtraInfos;
  const extraInfosDone = new Promise(resolve => {
    resolveExtraInfos = resolve;
  });

  function maybeResolve() {
    if (requestWillBeSents.length === 3 &&
        requestWillBeSentExtraInfos.length === 3 &&
        responseReceivedExtraInfos.length === 3 &&
        responseReceiveds.length === 1) {
      resolveExtraInfos();
    }
  }

  dp.Network.onRequestWillBeSent(event => {
    requestIds.add(event.params.requestId);
    requestWillBeSents.push(event.params);
    maybeResolve();
  });
  dp.Network.onResponseReceived(event => {
    requestIds.add(event.params.requestId);
    responseReceiveds.push(event.params);
    maybeResolve();
  });
  dp.Network.onRequestWillBeSentExtraInfo(event => {
    requestIds.add(event.params.requestId);
    requestWillBeSentExtraInfos.push(event.params);
    maybeResolve();
  });
  dp.Network.onResponseReceivedExtraInfo(event => {
    requestIds.add(event.params.requestId);
    responseReceivedExtraInfos.push(event.params);
    maybeResolve();
  });
  dp.Network.onLoadingFailed(event => {
    testRunner.fail(`Unexpected loading failure: ${event.params.errorText}`);
  });

  await session.evaluateAsync(`
    new Promise(resolve => {
      const xhr = new XMLHttpRequest();
      xhr.open('GET', 'http://redirect-one.test:8000/inspector-protocol/network/resources/redirect-1.php');
      xhr.onreadystatechange = () => {
        if (xhr.readyState === XMLHttpRequest.DONE) {
          resolve();
        }
      };
      xhr.send();
    })
  `);

  await extraInfosDone;

  function getHeaderCaseInsensitive(headers, name) {
    const lowerName = name.toLowerCase();
    for (const [key, value] of Object.entries(headers)) {
      if (key.toLowerCase() === lowerName) {
        return value;
      }
    }
    return undefined;
  }

  testRunner.log(`requestIds count: ${requestIds.size}`);
  testRunner.log(`requestWillBeSents: ${requestWillBeSents.length}`);
  testRunner.log(
      `requestWillBeSentExtraInfos: ${requestWillBeSentExtraInfos.length}`);
  testRunner.log(`responseReceiveds: ${responseReceiveds.length}`);
  testRunner.log(
      `responseReceivedExtraInfos: ${responseReceivedExtraInfos.length}`);

  const hops = requestWillBeSents.map((params, index) => {
    const hasExtraResponseInfo = index < requestWillBeSents.length - 1 ?
        requestWillBeSents[index + 1].redirectHasExtraInfo :
        responseReceiveds[0]?.hasExtraInfo;
    return {
      url: params.request.url,
      hasExtraRequestInfo: Boolean(requestWillBeSentExtraInfos[index]),
      hasExtraResponseInfo:
          Boolean(hasExtraResponseInfo && responseReceivedExtraInfos[index]),
      requestHostHeader: getHeaderCaseInsensitive(
          requestWillBeSentExtraInfos[index]?.headers ?? {}, 'host'),
      responseXDevToolsRedirectHeader: getHeaderCaseInsensitive(
          responseReceivedExtraInfos[index]?.headers ?? {},
          'x-devtools-redirect'),
    };
  });
  testRunner.log(JSON.stringify(hops, null, 2));
  testRunner.completeTest();
});
