(async function(testRunner) {

 const {page, session, dp} = await testRunner.startURL(
      'resources/stale-while-revalidate.html',
      `Tests that checks for no crashes when stale-while-revalidate is on`);

  dp.Debugger.enable();
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  dp.Network.enable();
  dp.Network.setAttachDebugStack({enabled: true});
  await dp.Page.reload({ignoreCache: false});
  dp.Network.onRequestWillBeSent(request => {
    const url = request.params.request.url;
    if (/stale-script\.php$/.test(url)) {
      testRunner.log('Request Will be Sent for ' + url.substr(url.lastIndexOf('stale-script.php')));
    }
  });

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
