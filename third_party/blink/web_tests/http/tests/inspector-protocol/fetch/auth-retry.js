(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that re-trying auth works.`);

  await dp.Fetch.enable({handleAuthRequests: true});
  await dp.Page.enable();
  const navigationPromise = session.navigate('../network/resources/unauthorised.pl');

  const requestId = (await dp.Fetch.onceRequestPaused()).params.requestId;
  dp.Fetch.continueRequest({requestId});
  await dp.Fetch.onceAuthRequired();
  dp.Fetch.continueWithAuth({
    requestId,
    authChallengeResponse: {
      response: 'ProvideCredentials',
      username: 'lawrence',
      password: 'ken sent me'
    }
  });
  await dp.Fetch.onceAuthRequired();
  dp.Fetch.continueWithAuth({
    requestId,
    authChallengeResponse: {
      response: 'ProvideCredentials',
      username: 'TestUser',
      password: 'TestPassword'
    }
  });
  await navigationPromise;
  testRunner.log('PASSED: navigation complete.');
  testRunner.completeTest();
})
