(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests that the ad frame type is reported on navigation\n`);
  await dp.Page.enable();
  session.evaluate(`
    if (window.testRunner) {
      testRunner.setHighlightAds();
    }

    let ad_frame = document.createElement('iframe');
    document.body.appendChild(ad_frame);
    internals.setIsAdFrame(ad_frame.contentDocument);
    ad_frame.width = 100;
    ad_frame.height = 200;
    ad_frame.src = "about:blank";
  `);

  // The first navigation will occur before the frame is set as an ad frame.
  // So, we wait for the second navigation before logging the adFrameType.
  await dp.Page.onceFrameNavigated();
  const { params } = await dp.Page.onceFrameNavigated();
  testRunner.log({adFrameStatus: params.frame.adFrameStatus});
  testRunner.completeTest();
})
