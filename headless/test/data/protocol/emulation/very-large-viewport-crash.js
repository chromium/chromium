// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startWithFrameControl(
      `Tests that renderer survives emulation of a very large viewport.`);

  const response = await dp.Emulation.setDeviceMetricsOverride({
    deviceScaleFactor: 1,
    height: 1024,
    width: 10000000,
    mobile: false,
  });
  const virtualTimeTicksBase =
      (await dp.Emulation.setVirtualTimePolicy({policy: 'pause'}))
          .result.virtualTimeTicksBase;
  const virtualTimeChunk = 10 * 1000;
  const frameTimeTicks = 100;
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: virtualTimeChunk});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  await dp.HeadlessExperimental.beginFrame({
    frameTimeTicks: virtualTimeTicksBase + virtualTimeChunk,
    interval: frameTimeTicks,
    noDisplayUpdates: false});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: virtualTimeChunk});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  await dp.HeadlessExperimental.beginFrame({
    frameTimeTicks: virtualTimeTicksBase + 2 * virtualTimeChunk,
    interval: frameTimeTicks,
    noDisplayUpdates: true});
  testRunner.completeTest();
})
