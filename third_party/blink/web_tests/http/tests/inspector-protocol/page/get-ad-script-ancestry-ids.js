(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the script ancestry which caused the frame to be labelled as an ad is reported via Page.getAdScriptAncestryIds\n`);

  await dp.Page.enable();

  const adScriptUrl = testRunner.url('resources/ad-script.js');
  const transitiveAdScriptUrl = testRunner.url('resources/transitive-script.js');

  const transitiveAdFrameAttached = dp.Page.onceFrameAttached();

  session.evaluate(`
    testRunner.setDisallowedSubresourcePathSuffixes(["ad-script.js"], false /* block_subresources */);

    const adScriptUrl = "${adScriptUrl}";
    const transitiveAdScriptUrl = "${transitiveAdScriptUrl}";

    const adScript = document.createElement('script');
    adScript.src = adScriptUrl;
    document.body.appendChild(adScript);
  `);

  await transitiveAdFrameAttached;

  const {params} = await transitiveAdFrameAttached;

  const {result} = await dp.Page.getAdScriptAncestryIds({ frameId: params.frameId });

  testRunner.log('has adScriptAncestryIds via getAdScriptAncestryIds: ' + !!result.adScriptAncestryIds);
  testRunner.log('length: ' + result.adScriptAncestryIds.length);
  testRunner.completeTest();
})
