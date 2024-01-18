(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Don't crash reloading while paused in top-level await`);

  await dp.Debugger.enable();

  session.navigate('http://devtools.test:8000/inspector-protocol/debugger/reload-on-pause/resources/simple-script.php?script=top-level-await.js&type=module');
  await dp.Debugger.oncePaused();
  testRunner.log('Paused on debugger statement.');

  dp.Page.reload();
  await dp.Debugger.oncePaused();  // Second pause. We need to resume before we get the load event.
  await dp.Debugger.resume();
  await dp.Page.onceLoadEventFired();
  testRunner.log('Successfully reloaded.');

  testRunner.completeTest();
});
