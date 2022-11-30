// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests beforeunload dialog.`);

  await dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('/resources/beforeunload.html')});
  await dp.Page.onceLoadEventFired();

  // Click to activate beforeunload handling.
  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    buttons: 0,
    clickCount: 1,
    x: 1,
    y: 1,
  });
  await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    buttons: 0,
    clickCount: 1,
    x: 1,
    y: 1,
  });

  // Try closing first time.
  dp.Page.close();
  const dialog = await dp.Page.onceJavascriptDialogOpening();

  testRunner.log(dialog.params.type);

  dp.Page.handleJavaScriptDialog({ accept: false, });
  await dp.Page.javascriptDialogClosed();

  // Try closing second time. This will make sure that
  // the page didn't close after the first beforeunload dialog
  // was canceled.
  dp.Page.close();
  const dialog2 = await dp.Page.onceJavascriptDialogOpening();
  testRunner.log(dialog2.params.type);
  await dp.Page.handleJavaScriptDialog({ accept: true, }),

  testRunner.completeTest();
})
