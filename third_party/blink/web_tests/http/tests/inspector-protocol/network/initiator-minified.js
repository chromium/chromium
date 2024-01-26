(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/minified.html',
      `Tests that the initiator position is correct even when that initiator is minified.`);
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => testRunner.die('Timeout', errorForLog), 28000);

  dp.Network.enable();
  dp.Page.enable();
  dp.Page.reload({ignoreCache: true});

  let requests = [];
  dp.Network.onLoadingFailed(e => testRunnler.log(JSON.stringify(e)));
  await dp.Network.onceRequestWillBeSent(e => {
    requests.push(e.params);
    errorForLog = new Error(JSON.stringify(requests));
    // Wait for all expected requests to be done.
    return requests.length === 4;
  });

  requests = requests.filter(r => !r.request.url.endsWith('minified.html'))
                     .sort((a, b) => a.request.url.localeCompare(b.request.url));

  for (const {request, initiator} of requests) {
    testRunner.log(request.url);
    testRunner.log(`   ${initiator.url}:${initiator.lineNumber}:${initiator.columnNumber}`);
  }
  testRunner.completeTest();
})
