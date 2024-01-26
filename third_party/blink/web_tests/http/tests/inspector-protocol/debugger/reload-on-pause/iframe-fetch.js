(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Don't crash reloading while paused in the micro task queue of an iframe`);

  await dp.Debugger.enable();
  session.navigate('http://devtools.test:8000/inspector-protocol/debugger/reload-on-pause/resources/main.html');
  await dp.Debugger.oncePaused();
  testRunner.log('Paused on debugger statement.');

  dp.Page.reload();
  await dp.Page.onceLoadEventFired();
  testRunner.log('Successfully reloaded.');

  testRunner.completeTest();
});
