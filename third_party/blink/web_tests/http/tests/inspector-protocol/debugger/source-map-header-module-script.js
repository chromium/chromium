(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the SourceMap header is reported on Debugger.scriptParsed events`);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  dp.Runtime.enable();
  dp.Debugger.enable();

  let { resolve, promise: testDonePromise } = Promise.withResolvers();
  let scriptParsedEventCount = 0;
  function logScriptParsedEvent({ params: { url, sourceMapURL }}) {
    if (!url.includes('worker-setup.js')) {
      testRunner.log('Script parsed event:');
      testRunner.log(`  - url: ${url}`);
      testRunner.log(`  - sourceMapURL: ${sourceMapURL}`);
    }

    if (++scriptParsedEventCount === 3) {
      resolve();
    }
  }

  dp.Debugger.onScriptParsed(logScriptParsedEvent);

  page.navigate('http://devtools.test:8000/inspector-protocol/resources/source-map-module.html');

  const {params: {sessionId}} = await dp.Target.onceAttachedToTarget();
  const childSession = session.createChild(sessionId);
  childSession.protocol.Debugger.onScriptParsed(logScriptParsedEvent);
  childSession.protocol.Debugger.enable();

  await testDonePromise;

  testRunner.completeTest();
});
