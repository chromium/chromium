(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Emulation.setDataSaverOverride affects both navigator.connection.saveData and Save-Data request header.');

  const headerValue = async () => {
    const headers = await session.evaluateAsync(
        `fetch("${testRunner.url('resources/echo-headers.php')}").then(r => r.text())`);
    for (const header of headers.split('\n')) {
      if (header.toLowerCase().startsWith('save-data:')) {
        return header;
      }
    }
    return '<missing>';
  };

  await dp.Emulation.setDataSaverOverride({dataSaverEnabled: true});
  testRunner.log(`saveData after enable: ${await session.evaluate('navigator.connection.saveData')}`);
  testRunner.log(`Save-Data header after enable: ${await headerValue()}`);

  await dp.Emulation.setDataSaverOverride({dataSaverEnabled: false});
  testRunner.log(`saveData after disable: ${await session.evaluate('navigator.connection.saveData')}`);
  testRunner.log(`Save-Data header after disable: ${await headerValue()}`);

  await dp.Emulation.setDataSaverOverride({});
  testRunner.log(`saveData after reset: ${await session.evaluate('navigator.connection.saveData')}`);
  testRunner.log(`Save-Data header after reset: ${await headerValue()}`);

  testRunner.completeTest();
})
