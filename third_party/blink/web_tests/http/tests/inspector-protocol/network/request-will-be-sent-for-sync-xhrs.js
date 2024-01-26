(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Network.requestWillBeSent is dispatched for redirects inside sync XHRs`);

  await dp.Network.enable();
  await dp.Fetch.enable();

  dp.Network.onRequestWillBeSent(event => testRunner.log('Network.requestWillBeSent: ' + event.params.request.url));

  const url = testRunner.url('./initial');

  const evaluationPromise = session.evaluate(url => {
    const request = new XMLHttpRequest();
    request.open('GET', url, false);  // `false` makes the request synchronous
    request.send(null);
    return request.responseText;
  }, url);

  // Wait for initial request.
  const [interception1] = await Promise.all([
    dp.Fetch.onceRequestPaused(),
    dp.Network.onceRequestWillBeSent(),
  ]);
  await dp.Fetch.fulfillRequest({
    requestId: interception1.params.requestId,
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: testRunner.url('./redirect1')},
    ],
  });

  // First redirect should emit both Fetch.requestPaused and Network.requestWillBeSent.
  const [interception2] = await Promise.all([
    dp.Fetch.onceRequestPaused(),
    dp.Network.onceRequestWillBeSent(),
  ]);
  await dp.Fetch.fulfillRequest({
    requestId: interception2.params.requestId,
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: testRunner.url('./redirect2')},
    ],
  });

  // Second redirect should emit both Fetch.requestPaused and Network.requestWillBeSent.
  const [interception3] = await Promise.all([
    dp.Fetch.onceRequestPaused(),
    dp.Network.onceRequestWillBeSent(),
  ]);
  await dp.Fetch.fulfillRequest({
    requestId: interception3.params.requestId,
    responseCode: 200,
    responseHeaders: [],
    body: btoa('thisisxhrbody'),
  });

  testRunner.log('sync XHR body: ' + (await evaluationPromise));
  testRunner.completeTest();
})
