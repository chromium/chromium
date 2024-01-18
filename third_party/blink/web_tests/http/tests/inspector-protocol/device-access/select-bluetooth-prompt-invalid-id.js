(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests selecting device in bluetooth device request prompt with invalid parameters.`);

  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-test.js');
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
  const {id, devices: [{id: deviceId}]} = deviceRequestPrompted;

  {
    const result = await dp.DeviceAccess.selectPrompt(
        {id: '00000000000000000000000000000000', deviceId});
    testRunner.log(result, 'selectPrompt with invalid id: ');
  }
  {
    const result = await dp.DeviceAccess.selectPrompt(
        {id, deviceId: '00000000000000000000000000000000'});
    testRunner.log(result, 'selectPrompt with invalid device id: ');
  }

  await dp.DeviceAccess.cancelPrompt({id});
  await requestDevicePromise;

  testRunner.completeTest();
});
