(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that aborted navigation request does not result in navigation.`);

  await testRunner.browserP().Target.setDiscoverTargets({discover: true});
  await dp.Page.enable();
  var FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  var helper = new FetchHelper(testRunner, dp);
  await helper.enable(false);

  testRunner.browserP().Target.onTargetInfoChanged(
      () => testRunner.log('FAIL: got Target.onTargetInfoChanged'));

  helper.onceRequest().fail({ errorReason: 'Aborted' });

  let error = (await dp.Page.navigate({url: "http://www.example.com/"})).result.errorText;
  testRunner.log(`Error text from Page.navigate: ${error}`);
  const location = await session.evaluate('location.href');
  testRunner.log(`current location: ${location}`);
  testRunner.completeTest();
})
