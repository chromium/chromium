// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Test device orientation\n`);
  await TestRunner.addScriptTag('/resources/testharness.js');
  await TestRunner.evaluateInPagePromise(`
      var sensorProvider = null;
      var mockAlpha = 1.1;
      var mockBeta = 2.2;
      var mockGamma = 3.3;

      async function setUpDeviceOrientation()
      {
          const {sensorMocks, setMockSensorDataForType} =
              await import('/resources/sensor-helpers.js');
          sensorProvider = sensorMocks();
          let mockDataPromise = setMockSensorDataForType(
              sensorProvider,
              "RelativeOrientationEulerAngles",
              [mockBeta, mockGamma, mockAlpha]);
          window.addEventListener("deviceorientation", handler);
          return mockDataPromise;
      }

      function handler(evt)
      {
          console.log("alpha: " + evt.alpha + " beta: " + evt.beta + " gamma: " + evt.gamma);
      }

      function setUpOrientationSensor()
      {
          let orientationSensor = new RelativeOrientationSensor();
          orientationSensor.onreading = () =>
            console.log("quaternion: "
                + orientationSensor.quaternion[0].toFixed(6) + ','
                + orientationSensor.quaternion[1].toFixed(6) + ','
                + orientationSensor.quaternion[2].toFixed(6) + ','
                + orientationSensor.quaternion[3].toFixed(6));
          orientationSensor.start();
      }

      function cleanUpDeviceOrientation()
      {
          window.removeEventListener("deviceorientation", handler);
          sensorProvider.reset();
          return new Promise(done => setTimeout(done, 0));
      }
  `);

  TestRunner.runTestSuite([
    async function setUpDeviceOrientation(next) {
      await TestRunner.evaluateInPageAsync('setUpDeviceOrientation()');
      ConsoleTestRunner.addConsoleSniffer(next);
    },

    function firstOrientationOverride(next) {
      ConsoleTestRunner.addConsoleSniffer(next);
      TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(20, 30, 40);
    },

    function setUpOrientationSensor(next) {
      // The devtools inspector window needs to be focused to hear about
      // sensor changes.
      if (window.testRunner)
        testRunner.focusDevtoolsSecondaryWindow();
      TestRunner.evaluateInPage('setUpOrientationSensor()', next);
    },

    function secondOrientationOverride(next) {
      ConsoleTestRunner.addConsoleSniffer(next);
      TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(90, 0, 0);
    },

    async function clearOverride(next) {
      await TestRunner.DeviceOrientationAgent.clearDeviceOrientationOverride();
      await TestRunner.evaluateInPageAsync('cleanUpDeviceOrientation()');
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    },

    // This test ensures we do not repeatedly show console warning from
    // SensorInspectorAgent::SetOrientationSensorOverride() when Document
    // changes.
    async function reloadPageAndOverride(next) {
      // First, enable DeviceOrientationAgent again.
      TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(1, 2, 3);

      // Reload the page. DeviceOrientationInspectorAgent::Restore() will call
      // SensorInspectorAgent::SetOrientationSensorOverride() again, and we do
      // not want any new console messages.
      // The console message from the call to setDeviceOrientationOverride()
      // above is lost with the reload, but we do not care.
      await TestRunner.reloadPagePromise();
      await ConsoleTestRunner.dumpConsoleMessages();

      await TestRunner.DeviceOrientationAgent.clearDeviceOrientationOverride();
      next();
    },
  ]);
})();
