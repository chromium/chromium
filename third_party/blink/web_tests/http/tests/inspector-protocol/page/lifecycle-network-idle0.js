(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that ` +
      `Page.lifecycleEvent is issued after aborted client-side navigation.`);

  dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });

  const redirect_url = testRunner.url('resources/basic.html');

  await dp.Fetch.enable();
  dp.Fetch.onRequestPaused(event => {
    const {request, requestId} = event.params;
    testRunner.log(`Intercepted ${request.url}`);
    if (request.url === redirect_url) {
      dp.Fetch.failRequest({requestId, errorReason: 'Aborted'})
      return;
    }
    dp.Fetch.continueRequest({requestId});
  });
  dp.Page.navigate({
    url: testRunner.url(`resources/client-side-redirect.html?location=${redirect_url}`)
  });

  const networkIdle = await dp.Page.onceLifecycleEvent(event => event.params.name === 'networkIdle');
  testRunner.log(networkIdle);

  testRunner.completeTest();
})
