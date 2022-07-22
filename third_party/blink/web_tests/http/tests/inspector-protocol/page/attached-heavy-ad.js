(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the script which caused the frame to be labelled as an ad is reported on frame attachmend\n`);
  await dp.Page.enable();
  const firstFrameAttached = dp.Page.onceFrameAttached();
  session.evaluate(`
    ad_frame = document.createElement('iframe');
    document.body.appendChild(ad_frame);
    internals.setIsAdFrame(ad_frame);
  `);

  await firstFrameAttached;
  const secondFrameAttached = dp.Page.onceFrameAttached();
  session.evaluate(`
    ad_frame.src = 'javascript:document.body.appendChild(document.createElement("iframe"))'
  `);
  const {params} = await secondFrameAttached;
  testRunner.log('has adScriptId: ' + !!params.adScriptId);
  testRunner.completeTest();
})
