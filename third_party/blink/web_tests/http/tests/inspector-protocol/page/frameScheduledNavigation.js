(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests frameScheduledNavigation events when navigation is initiated in JS followed by other navigations.');

  let isLoading = false;
  dp.Page.onFrameStartedLoading(() => isLoading = true);
  dp.Page.onFrameStoppedLoading(() => isLoading = false);

  dp.Page.enable();
  session.evaluate(`
    var frame = document.createElement('iframe');
    document.body.appendChild(frame);
    frame.src = '${testRunner.url('resources/navigation-chain1.html')}';
  `);

  for (var i = 0; i < 6; i++) {
    var msg = await dp.Page.onceFrameScheduledNavigation();
    testRunner.log('Scheduled navigation with delay ' + msg.params.delay +
                   ' and reason ' + msg.params.reason + ' to url ' +
                   msg.params.url.split('/').pop());

    // This event should be received before the scheduled navigation is cleared,
    // unless the frame was already loading.
    if (!isLoading)
      await dp.Page.onceFrameStartedLoading();

    await dp.Page.onceFrameClearedScheduledNavigation();
    testRunner.log('Cleared scheduled navigation');
  }

  testRunner.completeTest();
})
