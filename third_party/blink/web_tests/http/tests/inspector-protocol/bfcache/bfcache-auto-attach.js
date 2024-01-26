(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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
  // Intentionally ignore evaluation errors - since we are navigating, we might
  // get the "Inspected target navigated or closed" error response.
  await dp.Runtime.evaluate({expression: 'window.history.back()'});
  targetInfo = (await attachedToTargetPromise).params.targetInfo;
  testRunner.log('OOPIF attached after BFCache navigation: ' + targetInfo.url);

  testRunner.completeTest();
});
