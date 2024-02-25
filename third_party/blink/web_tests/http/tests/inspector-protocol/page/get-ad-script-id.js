(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the script which caused the frame to be labelled as an ad is reported via Page.getAdScriptId\n`);
  await dp.Page.enable();
  const firstFrameAttached = dp.Page.onceFrameAttached();
  session.evaluate(`
    ad_frame = document.createElement('iframe');
    document.body.appendChild(ad_frame);
    internals.setIsAdFrame(ad_frame.contentDocument);
  `);

  await firstFrameAttached;
  const secondFrameAttached = dp.Page.onceFrameAttached();
  session.evaluate(`
    ad_frame.src = 'javascript:document.body.appendChild(document.createElement("iframe"))'
  `);
  const {params} = await secondFrameAttached;

  const { result } = await dp.Page.getAdScriptId({ frameId: params.frameId });
  testRunner.log('has adScriptId via getAdScriptId: ' + !!result.adScriptId);
  testRunner.completeTest();
})
