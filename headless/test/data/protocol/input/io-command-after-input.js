// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests sequence of input event processing.');

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://example.com/`,
      `<html>
      <button style="width: 200px; height: 200px"
          onclick="window.result = 'PASSED'">
        CLICK ME
      </button>
      </html>`);

  await dp.Runtime.enable();

  const {virtualTimeTicksBase} = (await dp.Emulation.setVirtualTimePolicy({
    policy: 'pause'})).result;
  await dp.Page.navigate({url: 'http://example.com'});

  // This test needs virtual time to consistently expose the problem.
  const virtualTimeBudget = 1000;
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: virtualTimeBudget});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  // Also, one frame to trigger DeferRendererTasksAfterInput logic.
  await dp.HeadlessExperimental.beginFrame({
    frameTimeTicks: virtualTimeTicksBase + virtualTimeBudget,
    interval: 100,
    noDisplayUpdates: false
  });
  await dp.Performance.enable();

  async function click(x, y) {
    dp.Input.dispatchMouseEvent({type: 'mouseMoved', x, y});
    await dp.Input.dispatchMouseEvent({
      type: 'mousePressed', x, y, button: 'left', buttons: 1, clickCount: 1});
    return dp.Input.dispatchMouseEvent({
      type: 'mouseReleased', x, y, button: 'left', buttons: 0});
  }
  await click(100, 100);

  // This gets dispatched on the IO thread.
  await dp.Performance.getMetrics();
  testRunner.log(await session.evaluate(`window.result`));
  testRunner.completeTest();
})
