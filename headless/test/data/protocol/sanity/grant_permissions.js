// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests grant permissions.');

  // Reset all permissions initially.
  await dp.Browser.resetPermissions();

  // Start listening for geolocation changes.
  await session.evaluateAsync(async () => {
    window.messages = [];
    window.subscriptionChanges = [];
    const result = await navigator.permissions.query({name: 'geolocation'});
    window.subscriptionChanges.push(`INITIAL 'geolocation': ${result.state}`);
    result.onchange = () => window.subscriptionChanges.push(
        `CHANGED 'geolocation': ${result.state}`);
  });

  testRunner.log('- Granting geolocation permission');
  await grant('geolocation');
  await waitPermission({name: 'geolocation'}, 'granted');

  testRunner.log('- Resetting all permissions');
  await dp.Browser.resetPermissions();
  await waitPermission({name: 'geolocation'}, 'denied');

  testRunner.log(await session.evaluate(() => window.subscriptionChanges));
  testRunner.log(await session.evaluate(() => window.messages));

  testRunner.completeTest();

  async function grant(...permissions) {
    const result = await dp.Browser.grantPermissions({permissions});
    if (result.error)
      testRunner.log(
          '- Failed to grant: ' + JSON.stringify(permissions) +
          '  error: ' + result.error.message);
    else
      testRunner.log('- Granted: ' + JSON.stringify(permissions));
  }

  async function waitPermission(descriptor, state) {
    await session.evaluateAsync(async (descriptor, state) => {
      const result = await navigator.permissions.query(descriptor);
      if (result.state && result.state === state)
        window.messages.push(`${JSON.stringify(descriptor)}: ${result.state}`);
      else
        window.messages.push(`Failed to set ${permission} to state: ${state}`);
    }, descriptor, state);
  }
})
