// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that DeviceOrientation and Emulation sensor overrides drive deviceorientation events and RelativeOrientationSensor readings, and that the reload warning is not repeated after a reload.`);

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  // Log console.log() calls from the page and console messages reported by
  // the backend (e.g. the DeviceOrientationInspectorAgent warning).
  let messageCount = 0;
  let messageWaiter = null;
  function onMessage(text) {
    testRunner.log(text);
    ++messageCount;
    if (messageWaiter && messageCount >= messageWaiter.count) {
      const {resolve} = messageWaiter;
      messageWaiter = null;
      resolve();
    }
  }
  function waitForMessages(count) {
    messageCount = 0;
    return new Promise(resolve => messageWaiter = {count, resolve});
  }
  dp.Runtime.onConsoleAPICalled(
      event => onMessage(`console.${event.params.type}: ${
          event.params.args.map(arg => arg.value).join(' ')}`));
  dp.Log.onEntryAdded(
      event => onMessage(`Log.entryAdded (${event.params.entry.level}): ${
          event.params.entry.text}`));
  await dp.Runtime.enable();
  await dp.Log.enable();
  await dp.Page.enable();
  // Sensor readings are only delivered to focused pages.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  const setUpPage = `
      var orientationSensor = new RelativeOrientationSensor;

      function setUpDeviceOrientation() {
        window.addEventListener('deviceorientation', handler);
      }

      function round(num) {
        if (num === null) {
          return 'null';
        }
        return num.toFixed(6);
      }

      function handler(evt) {
        console.log('alpha: ' + round(evt.alpha) + ' beta: ' + round(evt.beta) + ' gamma: ' + round(evt.gamma));
      }

      function setUpOrientationSensor() {
        orientationSensor.onreading = () =>
          console.log('quaternion: '
              + round(orientationSensor.quaternion[0]) + ','
              + round(orientationSensor.quaternion[1]) + ','
              + round(orientationSensor.quaternion[2]) + ','
              + round(orientationSensor.quaternion[3]));
        orientationSensor.start();
      }

      function cleanUpDeviceOrientation() {
        orientationSensor.stop();
        window.removeEventListener('deviceorientation', handler);
        return new Promise(done => setTimeout(done, 0));
      }
  `;
  await session.evaluate(setUpPage);

  await testRunner.runTestSuite([
    // The first two steps verify that it is possible to control the Device
    // Orientation API via the Emulation domain as well, and that in this case
    // no console message about reloading is reported.
    //
    // It is not possible to use the DeviceOrientation domain if the
    // "relative-orientation" sensor is overridden via the Emulation domain, so
    // we need to tear down the setup as well. The event listener will stop
    // receiving readings.
    async function setDeviceOrientationOverrideViaEmulationDomain() {
      await dp.Emulation.setSensorOverrideEnabled(
          {enabled: true, type: 'relative-orientation'});
      await dp.Emulation.setSensorOverrideReadings({
        type: 'relative-orientation',
        reading: {
          // This is equivalent to alpha=1.1, beta=2.2, gamma=3.3.
          quaternion: {x: 0.0189123, y: 0.0289715, z: 0.0101462, w: 0.9993498}
        }
      });
      const messages = waitForMessages(1);
      await session.evaluate('setUpDeviceOrientation()');
      await messages;
    },

    async function removeEmulationDomainSensorOverride() {
      // Wait for an event with null attributes to be fired once the sensor
      // override is disabled.
      const messages = waitForMessages(1);
      await dp.Emulation.setSensorOverrideEnabled(
          {enabled: false, type: 'relative-orientation'});
      await messages;
    },

    // Now override the device orientation data via the DeviceOrientation
    // domain. We need to wait for 2 messages: the console warning from
    // DeviceOrientationInspectorAgent and the console.log() call from
    // handler().
    async function firstOrientationOverride() {
      const messages = waitForMessages(2);
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 20, beta: 30, gamma: 40});
      await messages;
    },

    // Add a RelativeOrientationSensor to verify that it is also controlled by
    // the same virtual sensor. We wait until its onreading event handler is
    // invoked.
    async function setUpOrientationSensor() {
      const messages = waitForMessages(1);
      await session.evaluate('setUpOrientationSensor()');
      await messages;
    },

    // Change the orientation values again to check that both handler() and
    // |orientationSensor|'s onreading event handler are called (which is why
    // we need to wait for 2 messages).
    async function secondOrientationOverride() {
      const messages = waitForMessages(2);
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 90, beta: 0, gamma: 0});
      await messages;
    },

    async function clearOverride() {
      await session.evaluateAsync('cleanUpDeviceOrientation()');
      await dp.DeviceOrientation.clearDeviceOrientationOverride();
    },

    // This test ensures we do not repeatedly report the console warning from
    // SensorInspectorAgent::SetOrientationSensorOverride() when Document
    // changes.
    async function reloadPageAndOverride() {
      // First, enable the DeviceOrientation override again. The warning it
      // reports is not interesting here.
      const messages = waitForMessages(1);
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 3});
      await messages;

      // Reload the page. DeviceOrientationInspectorAgent::Restore() will call
      // DeviceOrientationInspectorAgent::SetOrientationSensorOverride() again,
      // and we do not want any new console messages.
      messageCount = 0;
      const loadEventPromise = dp.Page.onceLoadEventFired();
      await dp.Page.reload();
      await loadEventPromise;
      await session.evaluate('1 + 1');
      testRunner.log(`Messages after reload: ${messageCount}`);

      await dp.DeviceOrientation.clearDeviceOrientationOverride();
    },
  ]);
});
