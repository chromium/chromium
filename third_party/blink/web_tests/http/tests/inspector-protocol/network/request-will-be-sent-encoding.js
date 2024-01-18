(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure header values are correctly encoded as UTF8.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  dp.Network.onRequestWillBeSentExtraInfo(event => {
    testRunner.log(event.params.headers['Accept']);
    testRunner.log(event.params.headers['X-Test']);
    testRunner.completeTest();
  });

  await session.evaluate(`
  fetch('index.html', {
    method: 'get',
    headers: {
      'Accept': 'before-æøå-after',
      'X-Test': 'before-ß-after',
    }
  })
  .then(result => result.text())
  .then(console.log);
  `);
})
