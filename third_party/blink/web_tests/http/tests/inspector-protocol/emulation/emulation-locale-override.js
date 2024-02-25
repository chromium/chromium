(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests emulation of the default locale.');

  const defaultLocale = await session.evaluate(() => Intl.NumberFormat().resolvedOptions().locale);

  await dp.Emulation.setLocaleOverride({locale: 'ru_RU'});
  testRunner.log('\nSet locale to ru-RU');
  await printLocaleSpecificData();

  await page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/empty.html');
  testRunner.log('\nChecking locale after navigation');
  await printLocaleSpecificData();

  await page.navigate('http://localhost:8000/inspector-protocol/resources/empty.html');
  testRunner.log('\nChecking locale after cross-origin navigation');
  await printLocaleSpecificData();

  await dp.Emulation.setLocaleOverride({locale: 'zh_CN'});
  testRunner.log('\nSet locale to zh-CN');
  await printLocaleSpecificData();

  const sencondSession = await page.createSession();
  const secondOverride = await sencondSession.protocol.Emulation.setLocaleOverride({locale: 'en_GB'});
  testRunner.log('\nTried to set override from another session, got error:');
  testRunner.log(secondOverride.error.message);
  await sencondSession.disconnect();

  await dp.Emulation.setLocaleOverride();
  testRunner.log('\nReset locale to default');
  const currentLocale = await session.evaluate(() => Intl.NumberFormat().resolvedOptions().locale)
  testRunner.log('Default locale matches previous value: ' + (defaultLocale === currentLocale));

  const result = await dp.Emulation.setLocaleOverride({locale: '___'});
  testRunner.log('\nTried setting bogus locale:');
  testRunner.log(result.error.message);

  testRunner.completeTest();


  async function printLocaleSpecificData() {
    testRunner.log('Detected locale: ' + await session.evaluate(() => Intl.NumberFormat().resolvedOptions().locale));
    testRunner.log('Date locale string: ' + await session.evaluate(() => new Date('4/2/2020 20:02').toLocaleString()));
    // TODO(https://crrev.com/c/v8/v8/+/2049899): uncomment next line after V8 is updated
    // testRunner.log('Number locale string: ' + await session.evaluate(() => Number(10000.2).toLocaleString()));
  }
})
