(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests frameStartedNavigating events when navigation is initiated by renderer');

  await dp.Page.enable();

  dp.Page.onFrameStartedNavigating(event => {
    testRunner.log(event, `frameStartedNavigating`, testRunner.stabilizeNames,
        ['loaderId']);
  });

  dp.Page.onFrameRequestedNavigation(event => {
    // Log only the required parts of the event.
    testRunner.log({
      frameId: event.params.frameId,
      url: event.params.url
    }, `frameRequestedNavigation`);
  });

  // Share test page with `frame-requested-navigation` test.
  dp.Page.navigate(
      {url: testRunner.url('resources/frame-requested-navigation-1.html')});
  for (let i = 0; i < 6; ++i) {
    await dp.Page.onceFrameStartedNavigating();
  }

  testRunner.completeTest();
})
