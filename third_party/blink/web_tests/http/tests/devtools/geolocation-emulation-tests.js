// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that geolocation emulation with latitude and longitude works as expected.\n`);

  await TestRunner.navigatePromise('https://devtools.test:8443/devtools/network/resources/empty.html');
  await TestRunner.BrowserAgent.invoke_grantPermissions({
    origin: 'https://devtools.test:8443',
    permissions: ['geolocation']
  });
  await TestRunner.evaluateInPagePromise(`
    async function getPositionPromise() {
      try {
        const position = await new Promise((resolve, reject) => navigator.geolocation.getCurrentPosition(resolve, reject, { timeout: 1000 }));
        if ((new Date(position.timestamp)).toDateString() != (new Date()).toDateString())
            return 'Unexpected error occured: timestamps mismatch.';
        if (position && position.coords)
            return 'Latitude: ' + position.coords.latitude + ' Longitude: ' + position.coords.longitude;
        return 'Unexpected error occured. Test failed.';
      } catch (error) {
        const suffix = error.message ? ' (' + error.message + ')' : '';
        if (error.code === error.PERMISSION_DENIED)
          return 'Permission denied' + suffix;
        if (error.code === error.POSITION_UNAVAILABLE)
          return 'Position unavailable' + suffix;
        if (error.code === error.TIMEOUT)
          return 'Request timed out' + suffix;
        return 'Unknown error' + suffix;
      }
    }
  `);

  const positionBeforeOverride = await TestRunner.evaluateInPageAsync('getPositionPromise()');

  TestRunner.runTestSuite([
    async function testGeolocationUnavailable(next) {
      TestRunner.EmulationAgent.setGeolocationOverride();
      TestRunner.addResult(await TestRunner.evaluateInPageAsync('getPositionPromise()'));
      next();
    },

    async function testOverridenGeolocation(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(50, 100, 95);
      TestRunner.addResult(await TestRunner.evaluateInPageAsync('getPositionPromise()'));
      next();
    },

    async function testInvalidParam(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(true, 100, 95);
      next();
    },

    async function testInvalidGeolocation(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(200, 300, 95);
      TestRunner.addResult(await TestRunner.evaluateInPageAsync('getPositionPromise()'));
      next();
    },

    async function testNoOverride(next) {
      TestRunner.EmulationAgent.clearGeolocationOverride();
      var positionString = await TestRunner.evaluateInPageAsync('getPositionPromise()');
      if (positionString === positionBeforeOverride)
        TestRunner.addResult('Override was cleared correctly.');
      else
        TestRunner.addResult('Position differs from value before override.');

      // Reset browser context permissions after running this test.
      await TestRunner.BrowserAgent.invoke_resetPermissions();
      next();
    }
  ]);
})();

