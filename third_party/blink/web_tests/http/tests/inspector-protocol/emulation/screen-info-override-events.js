// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Screen emulation updates local Screen and ScreenDetails change events.');
  await page.navigate(
      'http://127.0.0.1:8000/inspector-protocol/resources/empty.html');
  await dp.Browser.grantPermissions({
    origin: 'http://127.0.0.1:8000',
    permissions: ['windowManagement'],
  });
  await session.evaluateAsync(async () => {
    window.details = await getScreenDetails();
    window.originalSize = [screen.width, screen.height];
    window.eventCounts = [0, 0, 0];
    screen.addEventListener('change', () => ++eventCounts[0]);
    details.addEventListener('currentscreenchange', () => ++eventCounts[1]);
    details.currentScreen.addEventListener('change', () => ++eventCounts[2]);
    window.armChanges = () => {
      window.changes = Promise.all([
        new Promise(resolve => screen.addEventListener(
                        'change',
                        () => {
                          resolve([screen.width, screen.height]);
                        },
                        {once: true})),
        new Promise(
            resolve => details.addEventListener('currentscreenchange',
                                                () => {
                                                  resolve([
                                                    details.currentScreen.width,
                                                    details.currentScreen.height
                                                  ]);
                                                },
                                                {once: true})),
        new Promise(resolve => details.currentScreen.addEventListener(
                        'change',
                        () => {
                          resolve([
                            details.currentScreen.width,
                            details.currentScreen.height
                          ]);
                        },
                        {once: true})),
      ]);
    };
  });

  await session.evaluate('armChanges()');
  const metrics = {
    width: 400,
    height: 300,
    deviceScaleFactor: 1,
    mobile: false,
    screenWidth: 733,
    screenHeight: 977,
  };
  await dp.Emulation.setDeviceMetricsOverride(metrics);
  testRunner.log('Apply event values: ' +
                 JSON.stringify(await session.evaluateAsync('changes')));
  // Reapplying identical metrics must not dispatch additional change events.
  await dp.Emulation.setDeviceMetricsOverride(metrics);

  await session.evaluate('armChanges()');
  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.log(
      'Reset restores all event values: ' +
      await session.evaluateAsync(
          'changes.then(values => values.every(value => ' +
          'value[0] === originalSize[0] && value[1] === originalSize[1]))'));
  testRunner.log('Event counts after apply, identical update and reset: ' +
                 JSON.stringify(await session.evaluate('eventCounts')));
  await dp.Browser.resetPermissions();
  testRunner.completeTest();
})
