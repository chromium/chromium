(function() {

class BluetoothHelper {
  constructor(testRunner, protocol) {
    this._testRunner = testRunner;
    this._protocol = protocol;
  }

  async setupFakeBluetooth() {
    const bluetoothMojom = await import(
        '/gen/device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.m.js');
    const contentMojom = await import(
        '/gen/content/web_test/common/fake_bluetooth_chooser.mojom.m.js');

    const fakeBluetooth = new bluetoothMojom.FakeBluetoothRemote();
    fakeBluetooth.$.bindNewPipeAndPassReceiver().bindInBrowser('process');
    await fakeBluetooth.setLESupported(true);

    const {fakeCentral: fakeBluetoothCentral} =
        await fakeBluetooth.simulateCentral(
            bluetoothMojom.CentralState.POWERED_ON);
    const simulateAdvertisementReceived = () =>
        (fakeBluetoothCentral.simulateAdvertisementReceived({
          deviceAddress: '01:23:45:67:89:AB',
          rssi: 127,
          scanRecord: {
            name: 'Device',
            appearance: {hasValue: false, value: 0},
            txPower: {hasValue: false, value: 0},
          }
        }));

    // Create a FakeBluetoothChooser to override the default chooser
    // WebTestContentBrowserClient would create.
    {
      const fakeBluetoothChooserFactoryRemote =
          new contentMojom.FakeBluetoothChooserFactoryRemote();
      fakeBluetoothChooserFactoryRemote.$.bindNewPipeAndPassReceiver()
          .bindInBrowser('process');

      const fakeBluetoothChooserPtr =
          new contentMojom.FakeBluetoothChooserRemote();
      const fakeBluetoothChooserClientReceiver =
          new contentMojom.FakeBluetoothChooserClientReceiver({onEvent() {}});
      await fakeBluetoothChooserFactoryRemote.createFakeBluetoothChooser(
          fakeBluetoothChooserPtr.$.bindNewPipeAndPassReceiver(),
          fakeBluetoothChooserClientReceiver.$.associateAndPassRemote());
    }

    return {fakeBluetooth, fakeBluetoothCentral, simulateAdvertisementReceived};
  }

  async evaluateRequestDevice() {
    const device = await this._protocol.Runtime.evaluate({
      expression: 'navigator.bluetooth.requestDevice({acceptAllDevices: true})',
      awaitPromise: true,
      userGesture: true
    });
    return device.result.result;
  }
}

return BluetoothHelper;
})()
