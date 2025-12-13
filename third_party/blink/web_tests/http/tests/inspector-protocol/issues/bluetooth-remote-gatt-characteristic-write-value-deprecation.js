(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that BluetoothRemoteGATTCharacteristic.writeValue() deprecation issues are reported`);
  const bp = testRunner.browserP();

  await dp.Audits.enable();

  await bp.BluetoothEmulation.enable({state: 'powered-on', leSupported: true});

  const address = '00:00:00:00:00:01';
  const serviceUuid = '0000180d-0000-1000-8000-00805f9b34fb';
  const characteristicUuid = '00002a37-0000-1000-8000-00805f9b34fb';

  await bp.BluetoothEmulation.simulatePreconnectedPeripheral({
    address: address,
    name: 'Write Device',
    manufacturerData: [],
    knownServiceUuids: [serviceUuid],
  });

  bp.BluetoothEmulation.onGattOperationReceived(async (e) => {
    if (e.params.type === 'connection') {
      await bp.BluetoothEmulation.simulateGATTOperationResponse({
        address: e.params.address,
        type: 'connection',
        code: 0,  // HCI_SUCCESS
      });
    } else if (e.params.type === 'discovery') {
      await bp.BluetoothEmulation.simulateGATTOperationResponse({
        address: e.params.address,
        type: 'discovery',
        code: 0,  // HCI_SUCCESS
      });
    }
  });

  const {result: {serviceId}} = await bp.BluetoothEmulation.addService({
    address: address,
    serviceUuid: serviceUuid,
  });

  await bp.BluetoothEmulation.addCharacteristic({
    serviceId: serviceId,
    characteristicUuid: characteristicUuid,
    properties: {
      write: true,
    },
  });

  bp.BluetoothEmulation.onCharacteristicOperationReceived(async (e) => {
    if (e.params.type === 'write') {
      await bp.BluetoothEmulation.simulateCharacteristicOperationResponse({
        characteristicId: e.params.characteristicId,
        type: 'write',
        code: 0,  // GATT_SUCCESS
      });
    }
  });

  const issuePromise = dp.Audits.onceIssueAdded();

  const evalPromise = dp.Runtime.evaluate({
    expression: `
    (async () => {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{services: ['${serviceUuid}']}]});
      const gatt = await device.gatt.connect();
      const service = await gatt.getPrimaryService('${serviceUuid}');
      const characteristic = await service.getCharacteristic(
        '${characteristicUuid}');
      await characteristic.writeValue(new Uint8Array([1]));
    })()`,
    awaitPromise: true,
    userGesture: true
  });

  const {params} = await issuePromise;

  testRunner.log(params, 'Inspector issue: ');
  await evalPromise;
  testRunner.completeTest();
})
