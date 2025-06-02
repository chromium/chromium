(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the script ancestry which caused the frame to be labelled as an ad is reported via Page.getAdScriptAncestry\n`);

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

  const {result} = await dp.Page.getAdScriptAncestry({ frameId: params.frameId });

  testRunner.log('has adScriptAncestry via getAdScriptAncestry: ' + !!result.adScriptAncestry);
  testRunner.log('ancestryChainLength: ' + result.adScriptAncestry.ancestryChain.length);
  testRunner.log('rootScriptFilterlistRule: ' + result.adScriptAncestry.rootScriptFilterlistRule);
  testRunner.completeTest();
})
