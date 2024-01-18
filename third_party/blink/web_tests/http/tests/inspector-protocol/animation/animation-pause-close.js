(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that the playback rate is reset on disabling.');

  dp.Animation.enable();
  await dp.Animation.setPlaybackRate({ playbackRate: 0 });
  testRunner.log((await dp.Animation.getPlaybackRate()).result.playbackRate);
  await dp.Animation.disable();
  await dp.Animation.enable();
  testRunner.log((await dp.Animation.getPlaybackRate()).result.playbackRate);
  testRunner.completeTest();
})
