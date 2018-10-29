(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that device emulation is not reset upon second client attach.');

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);

  await deviceEmulator.emulate(1200, 1000, 1);
  testRunner.log(await session.evaluate(`document.body.clientWidth + ' x ' + document.body.clientHeight`));
  testRunner.log('Creating a new session');
  await page.createSession();
  testRunner.log(await session.evaluate(`document.body.clientWidth + ' x ' + document.body.clientHeight`));
  testRunner.completeTest();
})
