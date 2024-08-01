(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that cookies for a different origin can be set`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({
    url: 'http://example.com',
    name: 'foo',
    value: 'bar1'
  }), 'Result of Network.setCookie: ');
  testRunner.log('Page cookies: '+ await helper.getCookiesLog(
    (await dp.Network.getCookies()).result
  ));
  testRunner.log('All cookies: ' + await helper.getCookiesLog());
  testRunner.completeTest();
});
