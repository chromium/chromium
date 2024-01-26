(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests canceling a bluetooth device request prompt after a device option is prompted.`);

  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-test.js')
  const helper = new BluetoothHelper(testRunner, dp);

  const {simulateAdvertisementReceived} = await helper.setupFakeBluetooth();
  await dp.DeviceAccess.enable();

  const requestDevicePromise = helper.evaluateRequestDevice();
  await dp.DeviceAccess.onceDeviceRequestPrompted();
  simulateAdvertisementReceived();
  const {params: deviceRequestPrompted} =
      await dp.DeviceAccess.onceDeviceRequestPrompted(
          ({params: {devices}}) => devices.length > 0);
  testRunner.log(deviceRequestPrompted, 'deviceRequestPrompted: ');
  const {id} = deviceRequestPrompted;

  const result = await dp.DeviceAccess.cancelPrompt({id});
  testRunner.log(result, 'cancelPrompt: ');

  testRunner.log(
      await requestDevicePromise, 'navigator.bluetooth.requestDevice: ');

  testRunner.completeTest();
});
