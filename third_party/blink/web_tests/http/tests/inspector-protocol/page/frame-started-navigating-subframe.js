(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests frameStartedNavigating events when navigation is initiated by cdp command');

  const navigationHelper = await testRunner.loadScript(
      '../resources/navigation-helper.js');

  let lastAttachedDp = dp;
  await navigationHelper.initProtocolRecursively(dp, session, (newDp) => {
    lastAttachedDp = newDp;
    newDp.Page.onFrameStartedNavigating(event => {
      testRunner.log(event, `frameStartedNavigating`, testRunner.stabilizeNames,
          ['loaderId']);
    });
  });

  await dp.Page.enable();

  testRunner.log("\nPrepare");
  dp.Page.navigate({url: testRunner.url('../resources/empty.html')});
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log("\nCreate an iframe");
  session.evaluateAsync(`
    var frame = document.createElement('iframe');
    frame.src = 'http://oopif.test:8000/inspector-protocol/resources/empty.html?1';
    document.body.appendChild(frame);
    new Promise(f => frame.onload = f)
  `);
  await navigationHelper.onceFrameStoppedLoading('resources/empty.html?1');

  testRunner.log("\nNavigate iframe");
  lastAttachedDp.Runtime.evaluate({
    expression: `window.location.href = '${testRunner.url(
        '../resources/empty.html?2')}'`
  });
  await navigationHelper.onceFrameStoppedLoading('resources/empty.html?2');

  testRunner.completeTest();
})
