(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `MutationObservers should keep firing after reloading while paused in a MutationObserver callback, with a same-origin popup open`);

  await dp.Page.enable();
  await dp.Debugger.enable();

  // The popup shares the agent (and therefore the MutationObserver delivery
  // state) with the opener, but is not destroyed by reloading the opener.
  await session.evaluate(`
    window.popup = window.open('about:blank');
    popup.observer = new popup.MutationObserver(() => {});
    popup.observer.observe(popup.document.body, {childList: true});

    window.target = document.body.appendChild(document.createElement('div'));
    new MutationObserver(() => {
      debugger;
    }).observe(target, {childList: true});
  `);

  dp.Runtime.evaluate(
      {expression: `target.appendChild(document.createElement('span'))`});
  await dp.Debugger.oncePaused();
  testRunner.log('Paused in MutationObserver callback.');

  dp.Page.reload();
  await dp.Page.onceLoadEventFired();
  testRunner.log('Reloaded.');

  const result = await session.evaluateAsync(`
    new Promise(resolve => {
      const target = document.body.appendChild(document.createElement('div'));
      new MutationObserver(() => resolve('MutationObserver fired.'))
          .observe(target, {childList: true});
      target.appendChild(document.createElement('span'));
      setTimeout(() => resolve('MutationObserver did not fire.'), 500);
    })
  `);
  testRunner.log(result);

  testRunner.completeTest();
});
