(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/cookie.pl',
      `Tests that raw response headers are not reported in case of site isolation.`);

  const responses = new Map();
  await dp.Network.enable();

  session.evaluate(`
      var script = document.createElement('script');
      script.src = 'cookie.pl';
      document.head.appendChild(script);`);
  dump(await dp.Network.onceResponseReceived());

  session.evaluate(`
      var script = document.createElement('script');
      script.src = 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/cookie.pl';
      document.head.appendChild(script);`);
  dump(await dp.Network.onceResponseReceived());
  testRunner.completeTest();

  function dump(response) {
    response = response.params.response;
    testRunner.log(`\n<script src="${response.url}">`);
    testRunner.log(`Set-Cookie: ${response.headers['Set-Cookie']}`);
  }
})
