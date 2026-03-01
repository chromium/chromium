(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnNewDocument does not create extra contexts');

  dp.Runtime.enable();
  dp.Page.enable();

  await dp.Page.navigate({ url: 'about:blank' });
  await dp.Page.addScriptToEvaluateOnNewDocument({ source: ``, worldName: `world` });

  const mainFrameId = (await dp.Page.getFrameTree()).result.frameTree.frame.id;
  await dp.Page.createIsolatedWorld({ frameId: mainFrameId, grantUniveralAccess: true, worldName: `world` });

  let counter = 0;
  dp.Runtime.onExecutionContextCreated(() => counter++);

  testRunner.log('Navigating');
  await dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/resources/image.html'});

  for (let i = 0; i < 10; i++) {
    // Give it some time to produce extra events.
    await session.evaluate(`1`);
  }

  testRunner.log('contexts: ' + counter);
  testRunner.completeTest();
})
