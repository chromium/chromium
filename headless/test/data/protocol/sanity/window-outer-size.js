// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests window outer ' +
      'size is properly adjusted upon Browser.setWindowSize.');

  const windowId = (await dp.Browser.getWindowForTarget()).result.windowId;
  const resizePromise = session.evaluateAsync(`
    new Promise(resolve =>
        {window.addEventListener('resize', resolve, {once: true})})
  `);
  dp.Browser.setWindowBounds({
    windowId,
    bounds: {
      windowState: 'normal',
      left: 100, top: 200, width: 700, height: 500
    }
  });

  await resizePromise;
  const size = (await session.evaluate(`
    ({outerWidth, outerHeight})
  `));

  testRunner.log(size, 'Outer window size: ');
  testRunner.completeTest();
})
