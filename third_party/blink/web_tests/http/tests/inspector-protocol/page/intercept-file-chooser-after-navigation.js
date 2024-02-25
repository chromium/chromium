(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.setInterceptFileChooserDialog state is preserved after cross-process navigation`);

  await dp.Page.enable();
  await dp.Runtime.enable();

  await page.navigate('http://localhost:8000/inspector-protocol/resources/empty.html');
  await dp.Page.setInterceptFileChooserDialog({enabled: true});
  testRunner.log('Navigating cross-process');
  await page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/empty.html');

  testRunner.log('Did navigate');

  const [event] = await Promise.all([
    dp.Page.onceFileChooserOpened(),
    session.evaluateAsyncWithUserGesture(async () => {
      const picker = document.createElement('input');
      picker.type = 'file';
      picker.click();
    })
  ]);
  testRunner.log('Intercepted file chooser in mode: ' + event.params.mode);

  testRunner.completeTest();
})
