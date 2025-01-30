(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests frameStartedNavigating on frameset');

  const navigationHelper = await testRunner.loadScript(
      '../resources/navigation-helper.js');

  await navigationHelper.initProtocolRecursively(dp, session, (newDp) => {
    newDp.Page.onFrameStartedNavigating(event => {
      testRunner.log(event, `frameStartedNavigating`, testRunner.stabilizeNames,
          ['loaderId']);
    });
  });

  await dp.Page.enable();

  testRunner.log("\nPrepare");
  dp.Page.navigate({url: testRunner.url('./resources/frameset.html')});
  await Promise.all([
    navigationHelper.onceFrameStoppedLoading('resources/frameset.html'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?spif'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?oopif')]);

  testRunner.log("\nFragment navigation");
  const current_url = (await dp.Runtime.evaluate(
      {expression: "location.href"})).result.result.value;
  await dp.Page.navigate({url: `${current_url}#some_fragment`});
  await Promise.all([
    navigationHelper.onceFrameStoppedLoading('resources/frameset.html'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?spif'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?oopif')]);

  testRunner.log("\nCross-document navigation");
  await dp.Page.navigate({url: `${current_url}?load_again`});
  await Promise.all([
    navigationHelper.onceFrameStoppedLoading(
        'resources/frameset.html?load_again'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?spif'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?oopif')]);

  testRunner.log("\nReload");
  await dp.Page.reload();
  await Promise.all([
    navigationHelper.onceFrameStoppedLoading(
        'resources/frameset.html?load_again'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?spif'),
    navigationHelper.onceFrameStoppedLoading('resources/basic.html?oopif')]);

  testRunner.completeTest();
})
