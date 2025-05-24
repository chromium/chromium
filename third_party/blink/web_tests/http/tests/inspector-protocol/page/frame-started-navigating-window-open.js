(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp, session } = await testRunner.startBlank(
    'Tests frameStartedNavigating events when navigation is initiated by cdp command');

  const navigationHelper = await testRunner.loadScript(
    '../resources/navigation-helper.js');

  await navigationHelper.initProtocolRecursively(testRunner.browserP(), testRunner.browserSession(), (newDp) => {
    newDp.Page.onFrameStartedNavigating(event => {
      testRunner.log(event, `frameStartedNavigating`, testRunner.stabilizeNames,
        ['loaderId']);
    });
  });

  await dp.Page.enable();

  testRunner.log("\nPrepare");
  dp.Page.navigate({ url: testRunner.url('../resources/empty.html') });
  await dp.Page.onceFrameStoppedLoading();


  dp.Runtime.evaluate({ expression: `window.open('${testRunner.url('../resources/empty.html?window.open')}')` });

  await navigationHelper.onceFrameStoppedLoading('resources/empty.html?window.open');

  dp.Runtime.evaluate({
    expression: `
        const link = document.createElement('a');
        link.href = '${testRunner.url("../resources/empty.html?link.click")}';
        link.target = '_blank';
        link.textContent = 'Click me!';
        document.body.appendChild(link);
        link.click();`,
    userGesture: true
  });

  // At this point, the new target navigation is expected to wait for being unblocked, but the
  // `Page.frameStartedNavigating` is emitted before it.
  await navigationHelper.onceFrameStoppedLoading('resources/empty.html?link.click')

  testRunner.completeTest();
})