(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that a fenced frame creates its own target with 'iframe' type.\n`);

  await dp.Target.setDiscoverTargets({discover: true});
  await session.evaluate(`
    let fencedframe = document.createElement('fencedframe');
    fencedframe.src = '../fenced-frame/resources/page-with-title.php';
    document.body.appendChild(fencedframe);
  `);

  const iframeTargets =
      (await dp.Target.getTargets())
          .result.targetInfos.filter(target => target.type === 'iframe');
  testRunner.log('SUCCESS: iframe Target count:' + iframeTargets.length);
  testRunner.completeTest();
})
