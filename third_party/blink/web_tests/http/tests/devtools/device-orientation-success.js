// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConsoleTestRunner} from 'console_test_runner';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Test device orientation\n`);
  await TestRunner.addScriptTag('/resources/testharness.js');
  // The page we navigate to does not really matter, we just want an origin we
  // control to pass to invoke_grantPermissions() below.
  await TestRunner.navigatePromise(
      'https://devtools.test:8443/devtools/network/resources/empty.html');
  await TestRunner.BrowserAgent.invoke_grantPermissions({
    origin: 'https://devtools.test:8443',
    permissions: ['sensors'],
  });

  await TestRunner.evaluateInPagePromise(`
      const orientationSensor = new RelativeOrientationSensor;

      function setUpDeviceOrientation()
      {
          window.addEventListener("deviceorientation", handler);
      }

      function round(num)
      {
          if (num === null) {
              return 'null';
          }
          return num.toFixed(6);
      }

      function handler(evt)
      {
          console.log("alpha: " + round(evt.alpha) + " beta: " + round(evt.beta) + " gamma: " + round(evt.gamma));
      }

      function setUpOrientationSensor()
      {
          orientationSensor.onreading = () =>
            console.log("quaternion: "
                + round(orientationSensor.quaternion[0]) + ','
                + round(orientationSensor.quaternion[1]) + ','
                + round(orientationSensor.quaternion[2]) + ','
                + round(orientationSensor.quaternion[3]));
          orientationSensor.start();
      }

      function cleanUpDeviceOrientation()
      {
          orientationSensor.stop();
          window.removeEventListener("deviceorientation", handler);
          return new Promise(done => setTimeout(done, 0));
      }
  `);

  TestRunner.runTestSuite([
    // The first two steps verify that it is possible to control the Device
    // Orientation API via the Emulation domain as well.
    //
    // They also verify that in this case no console message about reloading
    // DevTools is shown.
    //
    // It is not possible to use the DeviceOrientation domain if the
    // "relative-orientation" sensor is overridden via the Emulation domain, so
    // we need to tear down the setup as well. The event listener will stop
    // receiving readings.
    async function setDeviceOrientationOverrideViaEmulationDomain(next) {
      await TestRunner.EmulationAgent.setSensorOverrideEnabled(
          true, 'relative-orientation');
      await TestRunner.EmulationAgent.setSensorOverrideReadings(
          'relative-orientation', {
            // This is equivalent to alpha=1.1, beta=2.2, gamma=3.3.
            quaternion: {x: 0.0189123, y: 0.0289715, z: 0.0101462, w: 0.9993498}
          });
      await TestRunner.evaluateInPage('setUpDeviceOrientation()');
      ConsoleTestRunner.addConsoleSniffer(next);
    },

    async function removeEmulationDomainSensorOverride(next) {
      // Wait for an event with null attributes to be fired once the sensor
      // override is disabled.
      ConsoleTestRunner.addConsoleSniffer(next);
      await TestRunner.EmulationAgent.setSensorOverrideEnabled(
          false, 'relative-orientation');
    },

    // Now override the device orientatio data via the DeviceOrientation
    // domain. We need to wait for 2 console messages: the console warning from
    // DeviceOrientationInspectorAgent and the console.log() call from
    // handler().
    async function firstOrientationOverride(next) {
      ConsoleTestRunner.waitUntilNthMessageReceived(2, next);
      await TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(
          20, 30, 40);
    },

    // Add a RelativeOrientationSensor to verify that it is also controlled by
    // the same virtual sensor. We wait until its onreading event handler is
    // invoked.
    async function setUpOrientationSensor(next) {
      // The devtools inspector window needs to be focused to hear about
      // sensor changes.
      if (window.testRunner)
        testRunner.focusDevtoolsSecondaryWindow();
      ConsoleTestRunner.addConsoleSniffer(next);
      await TestRunner.evaluateInPage('setUpOrientationSensor()');
    },

    // Change the orientation values again to check that both handler() and
    // |orientationSensor|'s onreading event handler are called (which is why
    // we need to wait for 2 messages).
    async function secondOrientationOverride(next) {
      ConsoleTestRunner.waitUntilNthMessageReceived(2, next);
      await TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(
          90, 0, 0);
    },

    async function clearOverride(next) {
      await TestRunner.evaluateInPageAsync('cleanUpDeviceOrientation()');
      await TestRunner.DeviceOrientationAgent.clearDeviceOrientationOverride();
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    },

    // This test ensures we do not repeatedly show console warning from
    // SensorInspectorAgent::SetOrientationSensorOverride() when Document
    // changes.
    async function reloadPageAndOverride(next) {
      // First, enable DeviceOrientationAgent again.
      await TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(
          1, 2, 3);

      // Reload the page. DeviceOrientationInspectorAgent::Restore() will call
      // DeviceOrientationInspectorAgent::SetOrientationSensorOverride() again,
      // and we do not want any new console messages.
      // The console message from the call to setDeviceOrientationOverride()
      // above is lost with the reload, but we do not care.
      await TestRunner.reloadPagePromise();
      await ConsoleTestRunner.dumpConsoleMessages();

      await TestRunner.DeviceOrientationAgent.clearDeviceOrientationOverride();
      next();
    },
  ]);
})();
