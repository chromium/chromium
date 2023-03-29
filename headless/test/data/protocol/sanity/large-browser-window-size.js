// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests browser window size larger than desktop.`);

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await dp.Page.enable();

  const set_bounds = {left: 0, top: 0, width: 4096, height: 2160};
  await dp.Browser.setWindowBounds({windowId, bounds: set_bounds});
  testRunner.log(`set size: ${set_bounds.width}x${set_bounds.height}`);

  await dp.Page.onceFrameResized();

  const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
  testRunner.log(`get size: ${bounds.width}x${bounds.height}`);

  const value =
      await session.evaluate(`window.outerWidth + 'x' + window.outerHeight`);
  testRunner.log(`window size: ${value}`);

  testRunner.completeTest();
})
