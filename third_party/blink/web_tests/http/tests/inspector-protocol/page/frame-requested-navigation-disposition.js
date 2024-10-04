(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('resources/frame-requested-navigation-disposition.html', 'Tests that dispostion for client-requested navigation is properly reported.');

  await dp.Page.enable();

  // The `buttons` values below match the expectations in the PointerEvents
  // spec: https://w3c.github.io/pointerevents/#the-buttons-property
  testRunner.log('New window');
  const [{params: newWindow}] = await Promise.all([
    dp.Page.onceFrameRequestedNavigation(),
    dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      modifiers: 8,
      x: 5,
      y: 5,
    }),
    dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      buttons: 1,
      clickCount: 1,
      modifiers: 8,
      x: 5,
      y: 5,
    }),
  ]);
  testRunner.log(newWindow);

  testRunner.log('New tab');
  const [{params: newTab}] = await Promise.all([
    dp.Page.onceFrameRequestedNavigation(),
    dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'middle',
      buttons: 0,
      clickCount: 1,
      x: 5,
      y: 5,
    }),
    dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'middle',
      buttons: 4,
      clickCount: 1,
      x: 5,
      y: 5,
    }),
  ]);
  testRunner.log(newTab);

  testRunner.log('Current tab');
  const [{params: currentTab}] = await Promise.all([
    dp.Page.onceFrameRequestedNavigation(),
    dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      x: 5,
      y: 5,
    }),
    dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      buttons: 1,
      clickCount: 1,
      x: 5,
      y: 5,
    }),
  ]);
  testRunner.log(currentTab);

  testRunner.completeTest();
})
