(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(`Tests updating request URL to a cross-origin one.`);

  dp.Fetch.enable({});
  await dp.Network.enable();
  dp.Network.onLoadingFailed(e => {
    testRunner.log(e.params, 'FAIL: ');
  });

  const path = 'inspector-protocol/resources/image.html';
  const origin1 = 'http://domain1.test:8080/';
  const origin2 = 'http://domain2.test:8080/';
  testRunner.log('Initial navigation');
  const navigationPromise = session.navigate(new URL(path, origin1).href);
  dp.Fetch.onceRequestPaused(event => {
    const {requestId, request} = event.params;
    const url = new URL(new URL(request.url).pathname, origin2).href;
    testRunner.log(request.url + ' => ' + url);
    dp.Fetch.continueRequest({requestId, url});
  });

  await navigationPromise;
  dp.Page.enable();
  testRunner.log('Anchor navigation');
  session.evaluate(() => {
    const a = document.createElement('a');
    a.href = 'http://domain1.test:8080/inspector-protocol/resource/empty.html';
    a.textContent = 'Click Me!';
    document.body.appendChild(a);
    a.click();
  });

  await dp.Page.onceLoadEventFired();
  testRunner.completeTest();
})
