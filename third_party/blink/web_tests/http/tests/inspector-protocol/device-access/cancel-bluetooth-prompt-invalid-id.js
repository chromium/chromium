(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests canceling a bluetooth device request prompt with invalid parameters.`);

  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-test.js');
  const helper = new BluetoothHelper(testRunner, dp);

  await helper.setupFakeBluetooth();
  await dp.DeviceAccess.enable();

  const requestDevicePromise = helper.evaluateRequestDevice();
  const {params: deviceRequestPrompted} = await dp.DeviceAccess.onceDeviceRequestPrompted();
  testRunner.log(deviceRequestPrompted, 'deviceRequestPrompted: ');
  const {id} = deviceRequestPrompted;

  const result = await await dp.DeviceAccess.cancelPrompt(
      {id: '00000000000000000000000000000000'});
  testRunner.log(result, 'cancelPrompt with invalid id: ');

  await dp.DeviceAccess.cancelPrompt({id});
  await requestDevicePromise;

  testRunner.completeTest();
});
