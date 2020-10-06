(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Tests that the initiator position is correct even when that initiator is minified.`);

  dp.Network.enable();
  dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('resources/minified.html')});

  let requests = [];
  await dp.Network.onceRequestWillBeSent(e => {
    requests.push(e.params);
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
