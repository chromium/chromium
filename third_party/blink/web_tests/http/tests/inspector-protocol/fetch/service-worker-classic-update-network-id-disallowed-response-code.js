(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const test = await testRunner.loadScript(
      'resources/service-workers/service-worker-test.js');
  await test(testRunner, {
    description: 'disallowed response code',
    type: 'classic',
    responseCode: 400,
    update: true
  });
})
