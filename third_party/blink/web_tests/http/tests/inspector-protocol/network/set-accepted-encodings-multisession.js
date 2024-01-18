(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
    'https://127.0.0.1:8443/inspector-protocol/resources/empty.html',
    `Tests setting accepted encodings.`);
  const resourceURL = 'https://127.0.0.1:8443/inspector-protocol/network/resources/content-encoding.php';

  await dp.Network.enable();

  const injectNavigation = (session) => session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${resourceURL}';
    document.body.appendChild(iframe);
  `)

  const injectFetch = (session) => session.evaluate(`fetch('${resourceURL}')`);

  async function runTest(label, session, dp, injectResource) {
    injectResource(session);
    const response = (await dp.Network.onceLoadingFinished()).params;
    const content = await dp.Network.getResponseBody({requestId: response.requestId});
    testRunner.log(`[${label}] Server received Accept-Encoding header: ${content.result.body}`);
  }

  async function testSession(label, session) {
    testRunner.log(`[${label}] Testing overrides for navigation requests.`);
    await runTest(label, session, session.protocol, injectNavigation);
    testRunner.log(`[${label}] Testing overrides for fetch requests.`);
    await runTest(label, session, session.protocol, injectFetch);
  }

  testRunner.log('Applying setAcceptedEncodings override to session#1');
  await dp.Network.setAcceptedEncodings({encodings: ['br']});
  await testSession('session#1', session);

  const session2 = await page.createSession();
  const dp2 = session2.protocol;
  await dp2.Network.enable();
  testRunner.log('Testing overrides set by the session#1 from session#2');
  await testSession('session#2', session2);

  testRunner.log('Testing additional overrides set by the session#2');
  await dp2.Network.setAcceptedEncodings({encodings: ['gzip']});
  await testSession('session#1', session);
  await testSession('session#2', session2);

  testRunner.log('Disconnecting original session (which held the override)');
  await session.disconnect();
  await testSession('session#2', session2);

  testRunner.completeTest();
})
