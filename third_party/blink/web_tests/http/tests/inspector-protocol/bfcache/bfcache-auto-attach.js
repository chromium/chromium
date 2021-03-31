(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
    'http://localhost:8000/inspector-protocol/bfcache/resources/page-with-oopif.html',
    'Tests that target for oopif is attached after BFCache navigation');
  await dp.Page.enable();

  let attachedToTargetPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
  let targetInfo = (await attachedToTargetPromise).params.targetInfo;
  testRunner.log('OOPIF attached: ' + targetInfo.url);

  const detachedFromTargetPromise = dp.Target.onceDetachedFromTarget();
  await session.navigate('http://devtools.oopif.test:8000/inspector-protocol/resources/test-page.html');
  await detachedFromTargetPromise;
  testRunner.log('detached OOPIF');

  attachedToTargetPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate('window.history.back()');
  targetInfo = (await attachedToTargetPromise).params.targetInfo;
  testRunner.log('OOPIF attached after BFCache navigation: ' + targetInfo.url);

  testRunner.completeTest();
});
