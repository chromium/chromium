(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(`Tests that Network.setCookie validates input`);

  const helper = (await testRunner.loadScript('resources/cookie-helper.js'))(dp);

  await dp.Network.enable();

  const cookies = [
    {url: 'ht2tp://127.0.0.1', name: 'foo1', value: 'bar1'},
    {url: 'http://127.0.0.1', name: 'foo2\0\r\na', value: 'bar2'},
    {url: 'http://127.0.0.1', name: 'foo3', value: 'bar3', sourceScheme: "SomeInvalidValue"},
    {url: 'http://127.0.0.1', name: 'foo4', value: 'bar3', sourcePort: -1234},
    {url: '', name: 'foo4', value: 'bar3'},
  ];

  for (const cookie of cookies) {
    testRunner.log(await dp.Network.setCookie(cookie), `Result of Network.setCookie(${JSON.stringify(cookie)}): `);
  }

  testRunner.log(await helper.getCookiesLog());
  testRunner.completeTest();
});
