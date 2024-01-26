(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests requestPaused.redirectedRequestId.`);

  const redirect = 'http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl';
  const hop2 = 'http://127.0.0.1:8000/protocol/inspector-protocol-page.html';
  const hop1 = `${redirect}?${hop2}`;
  const hop0 = `${redirect}?${hop1}`;

  await dp.Fetch.enable();
  const navigationPromise = session.navigate(hop0);
  let expectedRedirectedRequestId = undefined;

  for (let i = 0; i < 3; ++i) {
    const params = (await dp.Fetch.onceRequestPaused()).params;
    testRunner.log(`requestPaused.redirectedRequesId at hop ${i}: ` +
        (expectedRedirectedRequestId === params.redirectedRequestId
            ? 'as expected'
            : `unexpected (${params.redirectedRequestId})`));
    const requestId = params.requestId;
    dp.Fetch.continueRequest({requestId});
    expectedRedirectedRequestId = requestId;
  }
  await navigationPromise;
  testRunner.log(await session.evaluate(`location.href`));
  testRunner.completeTest();
})
