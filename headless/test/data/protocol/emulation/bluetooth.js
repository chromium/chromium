// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  testRunner.log('Tests request bluetooth device headless.');
  const {result: {sessionId}} =
      await testRunner.browserP().Target.attachToBrowserTarget({});
  const {protocol: bProtocol} = new TestRunner.Session(testRunner, sessionId);
  const {result: {browserContextId}} =
      await bProtocol.Target.createBrowserContext();
  {
    // Create page.
    const {result: {targetId}} = await bProtocol.Target.createTarget(
        {browserContextId, url: 'about:blank'});
    const {result: {sessionId}} =
        await bProtocol.Target.attachToTarget({targetId, flatten: true});
    const {protocol: pProtocol} = new TestRunner.Session(testRunner, sessionId);

    // In order to use Web Bluetooth, we need to load page off HTTPS, so use
    // interception.
    const FetchHelper =
        await testRunner.loadScriptAbsolute('../fetch/resources/fetch-test.js');
    const helper = new FetchHelper(testRunner, pProtocol);
    helper.onceRequest('https://test.com/index.html')
        .fulfill(FetchHelper.makeContentResponse(`<html></html>`));
    await helper.enable();
    await pProtocol.Page.navigate({url: 'https://test.com/index.html'});

    // Simulate an adapter and a bluetooth device.
    await bProtocol.BluetoothEmulation.enable(
        {state: 'powered-on', leSupported: true});
    await bProtocol.BluetoothEmulation.simulatePreconnectedPeripheral({
      address: '09:09:09:09:09:09',
      name: 'Test BLE device',
      manufacturerData: [],
      knownServiceUuids: [],
    });

    // Prepare device request prompt handling.
    await pProtocol.DeviceAccess.enable();
    const deviceRequestPromptedPromise =
        pProtocol.DeviceAccess.onceDeviceRequestPrompted().then(
            ({params: {id, devices}}) => {
              const deviceId = devices[0].id;
              return pProtocol.DeviceAccess.selectPrompt({id, deviceId});
            })

    // Check if bluetooth is available.
    const {result: {result: {value: availability}}} =
        await pProtocol.Runtime.evaluate({
          expression: `navigator.bluetooth.getAvailability();`,
          awaitPromise: true,
        });
    testRunner.log(`Bluetooth is available: ${availability}`);

    // Request a bluetooth device.
    const requestDevicePromise = pProtocol.Runtime.evaluate({
      expression:
          `navigator.bluetooth.requestDevice({acceptAllDevices: true})` +
          `.then(d => d.name)`,
      awaitPromise: true,
      userGesture: true,
    });
    await deviceRequestPromptedPromise;
    const {result: {result: {value: deviceName}}} = await requestDevicePromise;
    testRunner.log(`Bluetooth device name: ${deviceName}\n`);
  }

  testRunner.completeTest();
});
