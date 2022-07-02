(async function(testRunner) {
  const test = await testRunner.loadScript(
      'resources/service-workers/service-worker-test.js');
  await test(testRunner, {
    description: 'disallowed content-type',
    type: 'classic',
    contentType: 'application/json',
    update: true
  });
})
