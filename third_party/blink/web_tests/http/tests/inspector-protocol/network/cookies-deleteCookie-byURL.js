(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that cookies for a different origin can be set`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({name: 'cookie1', value: '.domain', url: 'http://www.chromium.org/path'}),
    'Setting cookie by URL without expires: ');
  testRunner.log(await dp.Network.setCookie({name: 'cookie2', value: '.domain', url: 'http://www.chromium.org/path', expires: Date.now() + 1000}),
    'Setting cookie by URL with expires: ');
  testRunner.log('All cookies before deletion: ' + await helper.getCookiesLog());
  testRunner.log((await dp.Network.deleteCookies({url: 'http://www.chromium.org/path', name: 'cookie1'})).result,
    'Delete cookies: ');
  testRunner.log('All cookies after deletion: ' + await helper.getCookiesLog());
  testRunner.completeTest();
});
