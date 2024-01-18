(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that client security state is reported on requestWillBeSentExtraInfo.`);

  await dp.Network.enable();
  const dp2 = (await page.createSession()).protocol;
  await dp2.Network.enable();
  testRunner.log('Two sessions established and Network enabled on both');

  const promise1 = dp.Network.onceRequestWillBeSentExtraInfo();
  const promise2 = dp2.Network.onceRequestWillBeSentExtraInfo();

  await session.evaluate(`fetch('index.html');`);

  const [event1, event2] = await Promise.all([promise1, promise2]);
  testRunner.log(event1.params.clientSecurityState)
  testRunner.log(event2.params.clientSecurityState)


  testRunner.completeTest();
})
