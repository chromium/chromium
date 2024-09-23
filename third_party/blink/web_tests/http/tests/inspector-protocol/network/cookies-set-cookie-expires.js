(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that cookies with the expires attribute can be set`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'foo',
    value: 'bar1',
    expires: 0
  }), 'Result of Network.setCookie with expires=0: ');

  testRunner.log(await helper.getCookiesLog());

  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'baz',
    value: 'bar2',
    expires: Date.now() + 1000000
  }), 'Result of Network.setCookie with expires=now+1000000: ');

  testRunner.log(await helper.getCookiesLog());

  testRunner.completeTest();
});
