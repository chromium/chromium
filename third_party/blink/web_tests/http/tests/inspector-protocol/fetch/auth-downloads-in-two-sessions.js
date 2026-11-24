(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session: session1, dp: dp1} = await testRunner.startBlank(
      `Tests that an auth challenge for a download is reported to the session that intercepted it while another session intercepts a download of its own.`);
  const page2 = await testRunner.createPage();
  const session2 = await page2.createSession();
  const dp2 = session2.protocol;

  // The realm and the credentials this URL expects are unique to this test, so
  // credentials cached by a test that ran earlier cannot satisfy the challenge.
  const authUrl =
      'http://127.0.0.1:8000/loading/resources/basic-auth-testing.php' +
      '?realm=auth-downloads-in-two-sessions' +
      '&username=auth-downloads-in-two-sessions&password=never-sent';
  // The server delays its response to this download, so the download is still
  // waiting for response headers when the other one is challenged.
  const slowUrl =
      'http://127.0.0.1:8000/devtools/network/resources/resource.php?wait=3000';

  function startDownload(session, url) {
    session.evaluate(`
      (function() {
        const a = document.createElement('a');
        a.href = '${url}';
        a.download = 'file.txt';
        document.body.appendChild(a);
        a.click();
      })()
    `);
  }

  await dp1.Fetch.enable({patterns: [{}], handleAuthRequests: true});
  await dp2.Fetch.enable({patterns: [{}], handleAuthRequests: true});

  // The first session's download will be challenged. Keep it paused until the
  // second session's download is in flight.
  startDownload(session1, authUrl);
  const first = (await dp1.Fetch.onceRequestPaused()).params;
  testRunner.log(`[session 1] Request paused: ${first.request.url}`);

  startDownload(session2, slowUrl);
  const second = (await dp2.Fetch.onceRequestPaused()).params;
  testRunner.log(`[session 2] Request paused: ${second.request.url}`);
  await dp2.Fetch.continueRequest({requestId: second.requestId});

  const authPromise = Promise.race([
    dp1.Fetch.onceAuthRequired().then(
        event => ({session: 1, dp: dp1, params: event.params})),
    dp2.Fetch.onceAuthRequired().then(
        event => ({session: 2, dp: dp2, params: event.params})),
  ]);
  await dp1.Fetch.continueRequest({requestId: first.requestId});

  const auth = await authPromise;
  testRunner.log(
      `[session ${auth.session}] Auth required: ${auth.params.request.url}`);
  const matches =
      auth.session === 1 && auth.params.requestId === first.requestId;
  testRunner.log(
      `Auth challenge is reported for the challenged download: ${matches}`);
  await auth.dp.Fetch.continueWithAuth({
    requestId: auth.params.requestId,
    authChallengeResponse: {response: 'CancelAuth'}
  });

  testRunner.completeTest();
})
