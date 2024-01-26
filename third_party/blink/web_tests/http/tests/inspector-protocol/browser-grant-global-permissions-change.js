(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Test that change callback is invoked during permission emulation`);

  // Reset all permissions initially.
  await dp.Browser.resetPermissions();

  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  await session.evaluateAsync(async () => {
    globalThis.events = [];
    return navigator.permissions
      .query({ name: 'geolocation' })
      .then(function (result) {
        globalThis.events.push(result.state);
        result.onchange = function () {
          globalThis.events.push(result.state);
        };
      });
  });

  await dumpChangeEvents();
  await grant('http://devtools.test:8000', 'geolocation');
  await dumpChangeEvents();
  await dp.Browser.resetPermissions();
  await dumpChangeEvents();

  testRunner.completeTest();

  async function grant(origin, ...permissions) {
    await dp.Browser.grantPermissions({ origin, permissions });
  }

  async function dumpChangeEvents() {
    testRunner.log(await session.evaluateAsync(async () => {
      return globalThis.events;
    }));
  }
});
