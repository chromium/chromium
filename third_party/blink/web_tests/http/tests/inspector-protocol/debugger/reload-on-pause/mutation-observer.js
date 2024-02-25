(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `MutationObserver should keep firing after reloading from a debugger statement`);

  await dp.Debugger.enable();

  session.navigate('http://devtools.test:8000/inspector-protocol/debugger/reload-on-pause/resources/simple-script.php?script=observer.js');
  await dp.Debugger.oncePaused();
  testRunner.log('Paused on debugger statement.');

  dp.Page.reload();
  await dp.Debugger.oncePaused(),
  testRunner.log('Successfully reloaded and paused.');

  testRunner.completeTest();
});
