(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that calling Page.captureScreenshot with optimizeForSpeed = true has some effect`);

  const slow = (await dp.Page.captureScreenshot({format: 'png'})).result.data;
  const fast = (await dp.Page.captureScreenshot({format: 'png', optimizeForSpeed: true})).result.data;

  const msg = slow.length < fast.length
      ? 'PASSED: slowly encoded PNG is smaller than quickly encoded'
      : `FAILED: ${slow.length} >= ${fast.length}`;
  testRunner.log(msg);
  testRunner.completeTest();
})
