(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Tests that Frame objects include the registered domain of the frames URL`);

  await dp.Page.enable();

  page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  const notification = await dp.Page.onceFrameNavigated();
  testRunner.log('Reported registered domain: ' + notification.params.frame.domainAndRegistry);
  testRunner.completeTest();
})
