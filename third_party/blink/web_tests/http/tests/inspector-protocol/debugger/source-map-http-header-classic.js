(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that SourceMap and X-SourceMap HTTP headers are propagated on Debugger.scriptParsed events.');

  await dp.Debugger.enable();

  const seenScripts = [];
  dp.Debugger.onScriptParsed(event => {
    const {url, sourceMapURL} = event.params;
    if (!url.endsWith('.php')) {
      return;
    }
    seenScripts.push({url, sourceMapURL});
  });

  await session.navigate('resources/source-map-http-header.html');

  if (seenScripts.length !== 2) {
    testRunner.fail(`Expected 2 .php scripts, got ${seenScripts.length}`);
    return;
  }

  for (const script of seenScripts) {
    testRunner.log('Added script:');
    testRunner.log(`  url: ${script.url}`);
    testRunner.log(`  sourceMapUrl: ${script.sourceMapURL}`);
  }

  testRunner.completeTest();
});
