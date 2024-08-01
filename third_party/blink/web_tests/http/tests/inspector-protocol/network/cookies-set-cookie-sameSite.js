(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that sameSite cookies can be set`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'foo1',
    value: 'bar1',
    sameSite: 'Lax'
  }), 'Result of Network.setCookie: ');
  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'foo2',
    value: 'bar2',
    sameSite: 'Strict'
  }), 'Result of Network.setCookie: ');
  testRunner.log(await helper.getCookiesLog());
  testRunner.completeTest();
});
