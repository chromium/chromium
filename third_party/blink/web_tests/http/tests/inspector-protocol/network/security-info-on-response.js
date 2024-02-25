(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/cookie.pl',
      `Tests that security details are reported, even in the case of site isolation.`);

  const responses = new Map();
  await dp.Network.enable();

  session.evaluate(`
      var script = document.createElement('script');
      script.src = 'https://devtools.oopif.test:8443/inspector-protocol/network/resources/cookie.pl';
      document.head.appendChild(script);`);
  const response = (await dp.Network.onceResponseReceived()).params.response;

  // The expected securityState is "insecure" because HTTPS resources are loaded with certificate errors in layout tests.
  // If security details weren't being reported at all, it would be "unknown".
  testRunner.log(`\nSecurity state: ${response.securityState}`);
  if (!response.securityDetails.protocol)
    testRunner.log(`FAIL: No TLS protocol in securityDetails`);
  // Sanity-check that raw headers are stripped when site isolation is enabled.
  testRunner.log(`Set-Cookie: ${response.headers['Set-Cookie']}`);

  testRunner.completeTest();
})
