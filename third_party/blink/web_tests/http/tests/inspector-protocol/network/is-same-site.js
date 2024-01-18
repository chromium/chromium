(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.oopif.test:8443/',
      `Verifies that same site requests are marked as such`);

  await dp.Network.enable();

  let gotAllRequests = null;
  const gotAllRequestsPromise =
      new Promise(resolve => {gotAllRequests = resolve});
  let requests = [];
  const onRequestWillBeSent = (event) => {
    requests.push(event.params.request);
    if (requests.length == 9) {
      gotAllRequests();
    }
  };
  dp.Network.onRequestWillBeSent(onRequestWillBeSent);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  dp.Target.onAttachedToTarget(async event => {
    const dp2 = session.createChild(event.params.sessionId).protocol;
    await dp2.Network.enable();
    dp2.Network.onRequestWillBeSent(onRequestWillBeSent);
    await dp2.Runtime.runIfWaitingForDebugger();
  });


  session.navigate(testRunner.url('./resources/same-site-root.html'));
  await gotAllRequestsPromise;

  requests.sort((a,b) => (a.url + a.headers.Referer).localeCompare(b.url + b.headers.Referer));
  testRunner.log(requests.map(r => ({url: r.url, isSameSite: r.isSameSite})), 'requests');

  testRunner.completeTest();
})
