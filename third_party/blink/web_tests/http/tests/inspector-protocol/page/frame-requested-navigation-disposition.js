(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL('resources/frame-requested-navigation-disposition.html', 'Tests that dispostion for client-requested navigation is properly reported.');

  await dp.Page.enable();

  testRunner.log('New window');
  const [{params: newWindow}] = await Promise.all([
    dp.Page.onceFrameRequestedNavigation(),
    dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 1,
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
      buttons: 2,
      clickCount: 1,
      x: 5,
      y: 5,
    }),
    dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'middle',
      buttons: 2,
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
      buttons: 1,
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
