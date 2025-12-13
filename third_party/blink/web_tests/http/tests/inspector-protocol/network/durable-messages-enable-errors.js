(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Ensures that DurableMessages cannot be enabled twice`);

  // Durable messages require a maxTotalBufferSize set.
  const ret1 = await dp.Network.enable({enableDurableMessages: true});
  testRunner.log(ret1);

  const ret2 = await dp.Network.enable(
    {maxTotalBufferSize: 115025, enableDurableMessages: true});
  testRunner.log(ret2);

  testRunner.completeTest();
})
