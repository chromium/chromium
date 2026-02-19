(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(
    'Tests that DurableMessages global limits can be configured across sessions.');
  const bp = testRunner.browserP();

  async function logDigest(str) {
    if (!str) {
      testRunner.log("Empty content");
      return;
    }
    const digest = await crypto.subtle.digest("SHA-256",
      new TextEncoder().encode(str));
    testRunner.log(new Uint8Array(digest).toBase64());
  }

  async function logResponseBodyAndGetId(session, dp, url) {
    session.evaluate(`fetch("${url}").then(r => r.text());`);

    const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
    testRunner.log(`Request for ${requestWillBeSent.request.url.split('/').pop()}`);

    await dp.Network.onceLoadingFinished(
      e => e.params.requestId === requestWillBeSent.requestId);
    return requestWillBeSent.requestId;
  }

  // Create the first page and enable Network.
  const page1 = await testRunner.createPage();
  const page1Session = await page1.createSession();
  const dp1 = page1Session.protocol;
  dp1.Network.enable();

  // Create a second page and enable Network.
  const page2 = await testRunner.createPage();
  const page2Session = await page2.createSession();
  const dp2 = page2Session.protocol;
  dp2.Network.enable();

  // Create a third page to query from later.
  const page3 = await testRunner.createPage();
  const page3Session = await page3.createSession();
  const dp3 = page3Session.protocol;
  dp3.Network.enable();

  // Configure Durable Messages to have a limit of 30,000 bytes.
  // The global limit is set via Virtual test suite flags.
  await dp1.Network.configureDurableMessages({
    maxTotalBufferSize: 20000
  });

  // Second page configures with a larger local limit.
  await dp2.Network.configureDurableMessages({
    maxTotalBufferSize: 30000
  });

  // Load a resource (~12KB) on the first page.
  const abe = await logResponseBodyAndGetId(
    page1Session, dp1, testRunner.url('./resources/abe.png'));

  // Load another resource (~12KB) on the second page.
  // Since page sessions in this test runner share the same Browser root_session_, and
  // thus the exact same DevtoolsDurableMessageCollector, this will hit the global limit of
  // 20KB for that single collector. The collector should evict the oldest message (abe) and
  // keep the new one (abe2).
  const abe2 = await logResponseBodyAndGetId(
    page2Session, dp2, testRunner.url('./resources/abe.png?v=2'));

  testRunner.log('Closing page1 and page2');
  await dp1.Page.close();
  await dp2.Page.close();

  testRunner.log('Retrieving responses from Collector via page3');

  // Retrieve the first resource via dp3. It should have been evicted due to global limit.
  const dataAbe = await dp3.Network.getResponseBody({requestId: abe});
  if (dataAbe.error) {
    testRunner.log('Error retrieving first resource: ' + dataAbe.error.message);
  } else {
    testRunner.log('Successfully retrieved first resource.');
  }

  // Retrieve the second resource via dp3. It should be available.
  const dataAbe2 = await dp3.Network.getResponseBody({requestId: abe2});
  if (dataAbe2.error) {
    testRunner.log('Error retrieving second resource: ' + dataAbe2.error.message);
  } else {
    testRunner.log('Successfully retrieved second resource.');
    await logDigest(dataAbe2.result.body);
  }

  testRunner.completeTest();
})
