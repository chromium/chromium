(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/resource.php?type=html',
      'Verifies that socket-layer throughput limits apply to the bytes on ' +
          'the wire.');
  const helper = (await testRunner.loadScript('resources/helper.js'))(
      testRunner, session, dp);
  await dp.Network.enable();

  // Clears the conditions before setting them, so that the next request gets a
  // new socket-layer session, and so a fresh bandwidth budget.
  async function setFreshConditions(conditions) {
    await helper.clearConditions();
    await helper.setConditions(conditions);
  }

  // Both responses are ~100KB, but the compressible one is only ~500 bytes on
  // the wire. At 20KB/s, they take ~5s and ~25ms.
  const downloadThroughput = 20 * 1024;
  const thresholdMs = 2500;
  await setFreshConditions({downloadThroughput});
  helper.logThrottling('Incompressible response',
                       await helper.measureFetch('incompressible'), true,
                       thresholdMs);
  await setFreshConditions({downloadThroughput});
  helper.logThrottling('Compressible response',
                       await helper.measureFetch('compressible'), false,
                       thresholdMs);

  // Uploads are limited the same way: at 20KB/s, a ~100KB request body takes
  // ~5s, while a small one fits in the throttle's initial burst.
  const uploadThroughput = 20 * 1024;
  await setFreshConditions({uploadThroughput});
  helper.logThrottling('Large request body',
                       await helper.measureUpload(100 * 1024), true,
                       thresholdMs);
  await setFreshConditions({uploadThroughput});
  helper.logThrottling('Small request body', await helper.measureUpload(100),
                       false, thresholdMs);

  testRunner.completeTest();
})
