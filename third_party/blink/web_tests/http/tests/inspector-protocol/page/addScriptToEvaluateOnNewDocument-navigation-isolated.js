(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that Page.addScriptToEvaluateOnNewDocument on works on navigations with isolated world.`);

  await dp.Page.enable();
  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(event => {
    testRunner.log(event, 'console called: ');
  });

  await dp.Page.addScriptToEvaluateOnNewDocument({
    source: 'console.log("evaluated")',
    worldName: 'isolated',
  });

  const loaded = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({
    url: testRunner.url('resources/empty.html')
  });
  await loaded;

  testRunner.completeTest();
});
