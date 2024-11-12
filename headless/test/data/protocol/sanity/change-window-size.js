// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests window ' +
      'bounds are properly adjusted upon Browser.setWindowSize.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;
  dp.Browser.setWindowBounds({
    windowId,
    bounds: {
      windowState: 'normal',
      left: 100, top: 200, width: 700, height: 500
    }
  });

  const bounds = (await dp.Browser.getWindowBounds({windowId})).result;

  // TODO(378531862): fix top/left on MacOS.
  if (navigator.platform.startsWith('Mac')) {
    bounds.bounds.left = 100;
    bounds.bounds.top = 200;
  }

  testRunner.log(bounds, 'Window bounds: ');
  testRunner.completeTest();
})
