(async function(testRunner) {
  const test = await testRunner.loadScript(
      'resources/service-workers/service-worker-test.js');
  await test(testRunner, {
    description: 'disallowed response code',
    type: 'module',
    responseCode: 400,
    update: true
  });
})
