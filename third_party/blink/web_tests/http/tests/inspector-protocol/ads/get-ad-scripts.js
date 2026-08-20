(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Ads.getAdScripts returns the known ad scripts with provenance.\n`);

  await dp.Page.enable();

  const adScriptUrl = testRunner.url('../page/resources/ad-script.js');
  const transitiveAdScriptUrl =
      testRunner.url('../page/resources/transitive-script.js');

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

  const {result} = await dp.Ads.getAdScripts();

  testRunner.log('scripts length: ' + result.newScripts.length);

  // Sort scripts by scriptId to make the output deterministic
  const sortedScripts =
      result.newScripts.sort((a, b) => a.scriptId.localeCompare(b.scriptId));

  const script1 = sortedScripts[0];
  if (script1?.provenance?.filterlistRule) {
    testRunner.log(`First script provenance: filterlistRule: ${
        script1.provenance.filterlistRule}`);
  }

  const script2 = sortedScripts[1];
  const ancestry = script2?.provenance?.adScriptAncestry;
  if (ancestry?.ancestryChain?.length === 1 &&
      ancestry.ancestryChain[0].scriptId === script1.scriptId) {
    testRunner.log(`Second script provenance: matches First script`);
  }

  testRunner.completeTest();
})
