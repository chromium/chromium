(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that an auth challenge for one of two concurrent downloads is reported for the download that was challenged.`);

  // The server delays its response to the first download, so that download is
  // still waiting for response headers when the second one is challenged.
  const slowUrl =
      'http://127.0.0.1:8000/devtools/network/resources/resource.php?wait=3000';
  // The realm and the credentials this URL expects are unique to this test, so
  // credentials cached by a test that ran earlier cannot satisfy the challenge.
  const authUrl =
      'http://127.0.0.1:8000/loading/resources/basic-auth-testing.php' +
      '?realm=auth-concurrent-downloads&username=auth-concurrent-downloads' +
      '&password=never-sent';

  function startDownload(url) {
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

  await dp.Fetch.enable({patterns: [{}], handleAuthRequests: true});

  startDownload(slowUrl);
  const first = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`Request paused: ${first.request.url}`);
  await dp.Fetch.continueRequest({requestId: first.requestId});

  // If the second download were not intercepted, only its auth challenge would
  // be reported, so wait for whichever of the two events comes first.
  const pausedPromise = dp.Fetch.onceRequestPaused();
  const authPromise = dp.Fetch.onceAuthRequired();
  startDownload(authUrl);
  const winner = await Promise.race([
    pausedPromise.then(event => ({paused: event.params})),
    authPromise.then(event => ({auth: event.params})),
  ]);
  let second = null;
  if (winner.paused) {
    second = winner.paused;
    testRunner.log(`Request paused: ${second.request.url}`);
    dp.Fetch.continueRequest({requestId: second.requestId});
  } else {
    testRunner.log(`FAIL: the second download was not intercepted`);
  }

  const auth = (await authPromise).params;
  testRunner.log(`Auth required: ${auth.request.url}`);
  const matches = !!second && auth.requestId === second.requestId;
  testRunner.log(
      `Auth challenge is reported for the second download: ${matches}`);
  await dp.Fetch.continueWithAuth({
    requestId: auth.requestId,
    authChallengeResponse: {response: 'CancelAuth'}
  });

  testRunner.completeTest();
})
