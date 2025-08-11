(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Testing that touch emulation works with multiple sessions');

  await dp.Emulation.setTouchEmulationEnabled({enabled: true, maxTouchPoints: 11});

  async function log(label) {
    const result = await session.evaluate(`window.matchMedia('(pointer: coarse)').matches`);
    testRunner.log(`${label} : ${result}`);
  }

  await log('before the second session created');

  await page.createSession();

  await log('after the second session');

  // Any URL would work.
  await page.navigate('/resources/blank.html')

  await log('after navigation session');

  testRunner.completeTest();
})
