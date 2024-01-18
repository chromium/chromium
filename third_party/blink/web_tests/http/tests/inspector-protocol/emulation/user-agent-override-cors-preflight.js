(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests emulation of the user agent for CORS preflight requests.');

  await dp.Network.enable();
  await dp.Emulation.setUserAgentOverride({userAgent: 'Nutri-Matic Drinks Dispenser'});
  testRunner.log('navigator.userAgent: ' + await session.evaluate('navigator.userAgent'));

  const url = 'http://localhost:8000/inspector-protocol/network/resources/cors-return-post.php';
  session.evaluate(`
      fetch("${url}", {method: 'POST', headers: {'X-DevTools-Test': 'foo'}, body: 'test'})
  `);
  const {headers} = (await dp.Network.onceRequestWillBeSentExtraInfo()).params;
  const headers_to_dump = [
    'Access-Control-Request-Headers',
    'User-Agent'
  ];
  for (const h of headers_to_dump)
    testRunner.log(`${h}: ${headers[h]}`);

  testRunner.completeTest();
})
