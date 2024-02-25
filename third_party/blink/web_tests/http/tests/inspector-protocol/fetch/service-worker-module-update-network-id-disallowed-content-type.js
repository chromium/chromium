(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const test = await testRunner.loadScript(
      'resources/service-workers/service-worker-test.js');
  await test(testRunner, {
    description: 'disallowed content-type',
    type: 'module',
    contentType: 'application/json',
    update: true
  });
})
