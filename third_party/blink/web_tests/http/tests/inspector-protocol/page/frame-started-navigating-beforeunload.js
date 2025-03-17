(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests frameStartedNavigating events with beforeunload');

  await dp.Page.enable();

  dp.Page.onFrameStartedNavigating(event => {
    testRunner.log(event, `frameStartedNavigating`, testRunner.stabilizeNames,
        ['loaderId']);
  });

  testRunner.log("\nPrepare");
  dp.Page.navigate(
      {url: testRunner.url('./resources/before-unload.html')});
  await dp.Page.onceFrameStoppedLoading();

  // User interacts with document for beforeunload to be triggered.
  await dp.Runtime.evaluate(
      {expression: `document.body.click()`, userGesture: true});

  testRunner.log("\nFragment navigation");
  // `current_url` is need to trigger fragment navigation, as
  // `testRunner.url(...)` can have relative path parts.
  const current_url = (await dp.Runtime.evaluate(
      {expression: "location.href"})).result.result.value;
  dp.Page.navigate({url: `${current_url}#some_fragment`});
  // `Page.frameStoppedLoading` is emitted for fragment navigation as well.
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log("\nReload");
  dp.Page.reload();
  await Promise.all([dp.Page.onceFrameStoppedLoading(),
    dp.Page.onceJavascriptDialogClosed()]);

  // User interacts with document for beforeunload to be triggered.
  await dp.Runtime.evaluate(
      {expression: `document.body.click()`, userGesture: true});

  testRunner.log("\nLeave beforeunload page");
  dp.Page.navigate({url: testRunner.url('../resources/empty.html')});
  await Promise.all([dp.Page.onceFrameStoppedLoading(),
    dp.Page.onceJavascriptDialogClosed()]);

  testRunner.completeTest();
})
