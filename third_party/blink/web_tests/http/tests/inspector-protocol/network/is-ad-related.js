(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that ad-related requests are marked as such.');

  await dp.Network.enable();

  const adScript = 'ad-script.js';

  session.evaluate(`
    testRunner.setDisallowedSubresourcePathSuffixes(['${
      adScript}'], /*block_subresources=*/false);
  `);

  for (const script of [adScript, 'script-1.js']) {
    session.evaluate(`fetch('resources/${script}')`);
    const e = await dp.Network.onceRequestWillBeSent();
    testRunner.log(`${script} isAdRelated: ${e.params.request.isAdRelated}`);
  }

  testRunner.completeTest();
})
