(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that permissions could be granted to all origins`);

  // Reset all permissions initially.
  await dp.Browser.resetPermissions();

  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  await grant(undefined, 'geolocation', 'audioCapture');
  await dumpPermission('geolocation');
  await dumpPermission('microphone');

  testRunner.log('Resetting all permissions');
  await dp.Browser.resetPermissions();
  await dumpPermission('geolocation');
  await dumpPermission('microphone');

  testRunner.log('Testing local permissions override global permissions');
  await grant(undefined, 'geolocation');
  await grant('http://devtools.test:8000', 'audioCapture');
  await dumpPermission('geolocation');
  await dumpPermission('microphone');

  testRunner.completeTest();

  async function grant(origin, ...permissions) {
    testRunner.log(`Grant ${permissions.join(' ')} to ${origin || 'all'}`);
    const response = await dp.Browser.grantPermissions({ origin, permissions });
    if (response.error)
      testRunner.log('    Failed to grant: ' + JSON.stringify(permissions) + '  error: ' + response.error.message);
    else
      testRunner.log('    Granted: ' + JSON.stringify(permissions));
  }

  async function dumpPermission(name) {
    testRunner.log(`Query ${name}`);
    const result = await session.evaluateAsync(async (permission) => {
      const result = await navigator.permissions.query({name: permission});
      return result.state;
    }, name);
    testRunner.log(`    => ${result}`);
  }
})
