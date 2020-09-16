(async function (testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests that the ad frame type is reported on navigation\n`);
  await dp.Page.enable();
  session.evaluate(`
    if (window.testRunner) {
      testRunner.setHighlightAds();
    }

    let ad_frame = document.createElement('iframe');
    document.body.appendChild(ad_frame);
    internals.setIsAdSubframe(ad_frame);
    ad_frame.width = 100;
    ad_frame.height = 200;
    ad_frame.src = "about:blank";
  `);

  // The first navigation will occur before the frame is set as an ad subframe.
  // So, we wait for the second navigation before logging the adFrameType.
  await dp.Page.onceFrameNavigated();
  const { params } = await dp.Page.onceFrameNavigated();
  testRunner.log({ adFrameType: params.frame.adFrameType });
  testRunner.completeTest();
})
