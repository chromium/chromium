(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      'Tests that Emulation.setDisabledImageTypes method does not crash the renderer.');

  await dp.Page.enable();

  await dp.Emulation.setDisabledImageTypes({imageTypes: ['avif', 'webp']});

  dp.Page.navigate(
      {url: testRunner.url('resources/disabled-image-types.html')});
  await dp.Page.onceLoadEventFired();

  testRunner.log('Reloading');
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();

  testRunner.log('Success');
  testRunner.completeTest();
})
