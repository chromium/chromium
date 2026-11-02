(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `MutationObservers should keep firing after terminating script execution while paused in a MutationObserver callback`);

  await dp.Debugger.enable();
  await session.evaluate(`
    window.target = document.body.appendChild(document.createElement('div'));
    window.pausedOnce = false;
    new MutationObserver(() => {
      if (!pausedOnce) {
        pausedOnce = true;
        debugger;
      }
    }).observe(target, {childList: true});
  `);

  dp.Runtime.evaluate(
      {expression: `target.appendChild(document.createElement('span'))`});
  await dp.Debugger.oncePaused();
  testRunner.log('Paused in MutationObserver callback.');

  // This is what DevTools does when reloading while paused, minus the
  // navigation.
  await dp.Debugger.resume({terminateOnResume: true});
  testRunner.log('Resumed with termination.');

  const result = await session.evaluateAsync(`
    new Promise(resolve => {
      new MutationObserver(() => resolve('MutationObserver fired.'))
          .observe(target, {childList: true});
      target.appendChild(document.createElement('span'));
      setTimeout(() => resolve('MutationObserver did not fire.'), 500);
    })
  `);
  testRunner.log(result);

  testRunner.completeTest();
});
