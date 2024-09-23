(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that cookies can be set`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({
    url: 'http://127.0.0.1',
    name: 'foo',
    value: 'bar1'
  }), 'Result of Network.setCookie: ');
  testRunner.log(await helper.getCookiesLog());
  testRunner.completeTest();
});
