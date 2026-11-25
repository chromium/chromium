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
    const provenance = e.params.request.adProvenance;
    testRunner.log(`${script} has adProvenance: ${provenance !== undefined}`);
    if (provenance) {
      testRunner.log(`${script} filterlistRule contains '${adScript}': ${
          Boolean(provenance.filterlistRule?.includes(adScript))}`);
      testRunner.log(`${script} has adScriptAncestry: ${
          provenance.adScriptAncestry !== undefined}`);
    }
  }

  // Test ad script ancestry provenance for a request initiated by an ad script.
  const adInitiatorScript = 'insert-image.js';
  session.evaluate(`
    testRunner.setDisallowedSubresourcePathSuffixes(['${
      adInitiatorScript}'], /*block_subresources=*/false);
  `);

  const scriptUrl = testRunner.url('../page/resources/insert-image.js');
  const imgEventPromise = dp.Network.onceRequestWillBeSent(
      e => e.params.request.url.includes('square200.png'));
  session.evaluate(`
    const script = document.createElement('script');
    script.src = '${scriptUrl}';
    document.body.appendChild(script);
  `);

  const imgEvent = await imgEventPromise;
  const imgReq = imgEvent.params.request;
  testRunner.log(`square200.png isAdRelated: ${imgReq.isAdRelated}`);
  testRunner.log(`square200.png has filterlistRule: ${
      imgReq.adProvenance?.filterlistRule !== undefined}`);
  testRunner.log(`square200.png has adScriptAncestry: ${
      imgReq.adProvenance?.adScriptAncestry !== undefined}`);
  if (imgReq.adProvenance?.adScriptAncestry) {
    const ancestry = imgReq.adProvenance.adScriptAncestry;
    testRunner.log(`square200.png ancestryChain length > 0: ${
        ancestry.ancestryChain.length > 0}`);
    testRunner.log(
        `square200.png ancestryChain[0].name contains '${adInitiatorScript}': ${
            ancestry.ancestryChain[0].name.includes(adInitiatorScript)}`);
    testRunner.log(`square200.png rootScriptFilterlistRule contains '${
        adInitiatorScript}': ${
        Boolean(
            ancestry.rootScriptFilterlistRule?.includes(adInitiatorScript))}`);
  }

  testRunner.completeTest();
})
