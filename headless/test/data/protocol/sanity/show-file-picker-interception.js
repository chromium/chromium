// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/body.html',
      'Tests that file picker interception works as expected');

  await dp.Page.enable();

  dp.Page.setInterceptFileChooserDialog({ enabled: true });

  // Test file picker APIs.
  const result = await session.evaluateAsyncWithUserGesture(`
    window.showOpenFilePicker();
  `);
  testRunner.log(result);

  const result2 = await session.evaluateAsyncWithUserGesture(`
    window.showSaveFilePicker();
  `);
  testRunner.log(result2);

  const result3 = await session.evaluateAsyncWithUserGesture(`
    window.showDirectoryPicker();
  `);
  testRunner.log(result3);

  // Test <input type="file"> element.
  const [event] = await Promise.all([
    dp.Page.onceFileChooserOpened(),
    session.evaluateAsyncWithUserGesture(async () => {
      const picker = document.createElement('input');
      picker.type = 'file';
      picker.click();
    })
  ]);
  testRunner.log('Intercepted file chooser mode: ' + event.params.mode);

  testRunner.completeTest();
})
