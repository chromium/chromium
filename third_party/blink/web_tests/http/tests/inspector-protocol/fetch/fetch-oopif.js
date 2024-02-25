(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests fetch interceptor respects cross-frame boundaries of out-of-process iframes.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');

  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();
  helper.onRequest().continueRequest();

  session.evaluate(`
      function addFrame(url) {
        const frame = document.createElement('iframe');
        frame.src = url;
        const promise = new Promise(fulfill => frame.addEventListener('load', fulfill));
        document.body.appendChild(frame);
        return promise;
      }
  `);

  testRunner.log('\nLoading in-process iframe (script should be present)');
  await session.evaluateAsync(`addFrame('${testRunner.url('resources/frame-with-subresource.html')}')`);

  testRunner.log('\nLoading out-of-process iframe (script should be absent)');
  await session.evaluateAsync(`addFrame('http://devtools.oopif-a.test:8000/inspector-protocol/fetch/resources/frame-with-subresource.html')`);

  testRunner.log('\nLoading out-of-process iframe with interception (script should be present)');
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    const dp1 = session.createChild(event.params.sessionId).protocol;
    const subframeFetcher = new FetchHelper(testRunner, dp1);
    subframeFetcher.setLogPrefix("[subframe] ");
    await subframeFetcher.enable();
    subframeFetcher.onRequest().continueRequest({});
    await dp1.Runtime.runIfWaitingForDebugger();
  });

  await session.evaluateAsync(`addFrame('http://devtools.oopif-a.test:8000/inspector-protocol/fetch/resources/frame-with-subresource.html')`);

  testRunner.completeTest();
})
