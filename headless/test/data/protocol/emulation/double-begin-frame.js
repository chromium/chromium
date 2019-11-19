// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  var {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests that beginFrame correctly reports an error ' +
      'with another pending frame.');

  await dp.Runtime.enable();

  const virtualTimeChunkSize = 100;
  const virtualTimeTicksBase =
     (await dp.Emulation.setVirtualTimePolicy({policy: 'pause'}))
         .result.virtualTimeTicksBase;
  dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending',
      budget: virtualTimeChunkSize});

  await dp.Emulation.onceVirtualTimeBudgetExpired();
  const frameTimeTicks = 100;
  dp.HeadlessExperimental.beginFrame({
      frameTimeTicks: virtualTimeTicksBase + frameTimeTicks,
      interval: virtualTimeChunkSize,
      noDisplayUpdates: false});
  const response = await dp.HeadlessExperimental.beginFrame({
    frameTimeTicks: virtualTimeTicksBase + frameTimeTicks,
    interval: virtualTimeChunkSize,
    noDisplayUpdates: false});

  testRunner.log(`response: ${JSON.stringify(response.error)}`);
  testRunner.completeTest();
})
