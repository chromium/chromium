(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests emulation with OOPIFs.');

  testRunner.log('Enabling auto-attach');
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false});

  testRunner.log('Navigating to page with OOPIF');
  await dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('../resources/site_per_process_main.html')});
  await dp.Page.onceLoadEventFired();

  testRunner.log('Emulating device');
  testRunner.log(await dp.Emulation.setDeviceMetricsOverride({width: 1201, height: 801, deviceScaleFactor: 2, mobile: true, screenWidth: 1402, screenHeight: 1401}));

  testRunner.log('screen.width:');
  testRunner.log(await session.evaluate(`screen.width`));

  testRunner.log('Reloading');
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();

  testRunner.log('screen.width:');
  testRunner.log(await session.evaluate(`screen.width`));

  testRunner.completeTest();
})
