(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

 const {page, session, dp} = await testRunner.startURL(
      'resources/stale-while-revalidate.html',
      `Tests that checks for no crashes when stale-while-revalidate is on`);

  dp.Debugger.enable();
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  dp.Network.enable();
  dp.Network.setAttachDebugStack({enabled: true});
  await dp.Page.reload({ignoreCache: false});

  let numResponses = 0;
  dp.Network.onResponseReceived(request => {
    const url = request.params.response.url;
    if (/stale-script\.php$/.test(url)) {
      ++numResponses;
      if (numResponses == 2) {
        testRunner.completeTest();
      }
    }
  });
})
