(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
      `Tests that session isn't suspended during a same-doc navigation intercepted by the navigate event with { commit: 'after-transition' });\n`);

  await dp.Page.enable();
  await session.evaluate(`
    navigation.onnavigate = e => {
      e.intercept({
        precommitHandler: async () => {
          await new Promise(r => setTimeout(r, 1000));
        }
      });
    };
  `);

  let navigate_promise = dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html#foo')});
  session.protocol.Runtime.evaluate({ expression: '' }).then(() => {
    testRunner.log('Evaluate finished!');
    testRunner.completeTest();
  });
  await Promise.all([navigate_promise,  dp.Page.onceNavigatedWithinDocument()])
  testRunner.completeTest();
});
