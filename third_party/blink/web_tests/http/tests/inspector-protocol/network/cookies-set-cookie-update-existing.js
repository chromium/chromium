(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that cookies can be updated`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'foo',
    value: 'bar1'
  }), 'Result of Network.setCookie: ');

  testRunner.log('Cookies before update: ' + await helper.getCookiesLog());

  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'foo',
    value: 'second bar2'
  }), 'Result of Network.setCookie to update existing one: ');

  testRunner.log('Cookies after update: ' + await helper.getCookiesLog());

  testRunner.completeTest();
});
