(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(
      'Tests that DurableMessages can collect across iframes and past page close');
  const bp = testRunner.browserP();

  // To reduce noise in the test output, we only log the SHA-256 digest of the
  // response body. However, one should ensure that the body content is verified
  // manually, while updating this test's expectation file.
  async function logDigest(str) {
    const digest = await crypto.subtle.digest("SHA-256",
      new TextEncoder().encode(str));
    testRunner.log(new Uint8Array(digest).toBase64());
  }

  async function logResponseBodyAndGetId(session, dp, url) {
    session.evaluate(`fetch("${url}").then(r => r.text());`);

    const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
    testRunner.log(`Request for ${requestWillBeSent.request.url}`);

    await dp.Network.onceResponseReceived();
    const data = await dp.Network.getResponseBody(
      {requestId: requestWillBeSent.requestId});
    await logDigest(data.result.body);
    return requestWillBeSent.requestId;
  }

  function createChildFrame(url) {
    return new Promise(resolve => {
      const frame = document.createElement(`iframe`);
      frame.src = url;
      frame.addEventListener('load', resolve);
      document.body.appendChild(frame);
    });
  }

  // Create the first page
  const page1 = await testRunner.createPage();
  const page1Session = await page1.createSession();
  const dp1 = page1Session.protocol;
  dp1.Network.enable();
  await dp1.Network.configureDurableMessages({maxTotalBufferSize: 115025});

  // Create a second page
  const page2 = await testRunner.createPage();
  const page2Session = await page2.createSession();
  const dp2 = page2Session.protocol;

  // This storage will be shared with the earlier one, so lower storage will be
  // ignored.
  dp2.Network.enable();
  await dp2.Network.configureDurableMessages({maxTotalBufferSize: 10});

  // Create an iframe on the first page
  await dp1.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const frame1Attached = dp1.Target.onceAttachedToTarget();
  page1Session.evaluate(`(${
      createChildFrame})('http://devtools.oopif.test:8080/inspector-protocol/resources/iframe.html')`);
  const frame1Session =
      page1Session.createChild((await frame1Attached).params.sessionId);
  const frame1Dp = frame1Session.protocol;

  // This storage will also be shared with the earlier one.
  frame1Dp.Network.enable();
  await frame1Dp.Network.configureDurableMessages({maxTotalBufferSize: 10});

  // Load a resource on the first page.
  const abe = await logResponseBodyAndGetId(
      page1Session, dp1, testRunner.url('./resources/abe.png'));
  // Load a resource in the iframe of first page.
  const css = await logResponseBodyAndGetId(
      frame1Session, frame1Dp,
      testRunner.url(
          'http://devtools.oopif.test:8080/inspector-protocol/network/resources/test.css'));
  // Load a resource in the second page.
  const js = await logResponseBodyAndGetId(
      page2Session, dp2, testRunner.url('./resources/final.js'));

  // Close first page.
  await dp1.Page.close();

  testRunner.log('Retrieving responses past first page close');

  // Retrieve resources from the first page and second page, via dp2
  const dataAbe = await dp2.Network.getResponseBody({requestId: abe});
  await logDigest(dataAbe.result.body);
  const dataCss = await dp2.Network.getResponseBody({requestId: css});
  await logDigest(dataCss.result.body);
  const dataJs = await dp2.Network.getResponseBody({requestId: js});
  await logDigest(dataJs.result.body);

  // Close the second page, that should cleanup the Durable Messages.
  await dp2.Page.close();

  testRunner.log('Retrieving responses past session cleanup');

  // Create a new, third page and configure Durable Messages.
  const page3 = await testRunner.createPage();
  const page3Session = await page3.createSession();
  const dp3 = page3Session.protocol;
  dp3.Network.enable();
  await dp3.Network.configureDurableMessages({maxTotalBufferSize: 115025});

  // Retrieve resources from the first page, via dp2, that after session is
  // closed, but still available via Durable Messages because the root session
  // is still open.
  const dataJs2 = await dp3.Network.getResponseBody({requestId: js});
  await logDigest(dataJs2.result.body);

  testRunner.log('DONE');
  testRunner.completeTest();
});
