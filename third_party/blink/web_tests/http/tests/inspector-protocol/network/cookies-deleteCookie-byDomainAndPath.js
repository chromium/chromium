(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that cookies for a different origin can be set`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  testRunner.log(await dp.Network.setCookie({name: 'cookie1', value: '.domain', domain: '.chromium.org', path: '/path'}),
    'Setting cookie by URL without expires: ');
  testRunner.log(await dp.Network.setCookie({name: 'cookie2', value: '.domain', domain: '.chromium.org', path: '/pathB', expires: Date.now() + 1000}),
    'Setting cookie by URL with expires: ');
  testRunner.log('All cookies before deletion: ' + await helper.getCookiesLog());
  testRunner.log((await dp.Network.deleteCookies({name: 'cookie1', domain: '.chromium.org', path: '/path'})).result,
    'Delete cookie1: ');
  testRunner.log((await dp.Network.deleteCookies({name: 'cookie2', domain: '.chromium.org', path: '/path'})).result,
    'Attempt to delete cookie2 with wrong path: ');
  testRunner.log('All cookies after deletion: ' + await helper.getCookiesLog());
  testRunner.completeTest();
});
