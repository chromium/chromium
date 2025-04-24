(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests Page.javascriptDialogOpening event');

  await dp.Page.enable();

  dp.Runtime.evaluate({expression: 'alert("Hello World")'});

  // FrameId is the same as targetId for the main frame.
  const expectedFrameId = page.targetId();

  const openingEvent = await dp.Page.onceJavascriptDialogOpening();
  testRunner.log(openingEvent, 'Page.javascriptDialogOpening: ');
  if (openingEvent.params.frameId === expectedFrameId) {
    testRunner.log('Page.javascriptDialogOpening has expected frameId');
  } else {
    testRunner.log('ERROR!!! Page.javascriptDialogOpening has unexpected frameId');
  }

  const closingEvent = await dp.Page.onceJavascriptDialogClosed();
  testRunner.log(closingEvent, 'Page.javascriptDialogClosed: ');
  if (closingEvent.params.frameId === expectedFrameId) {
    testRunner.log('Page.javascriptDialogClosed has expected frameId');
  } else {
    testRunner.log('ERROR!!! Page.javascriptDialogClosed has unexpected frameId');
  }

  testRunner.completeTest();
})
