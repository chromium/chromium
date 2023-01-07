// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that virtual time fence does not block interrupting protocol' +
      ' commands.');

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Performance.enable();
  await dp.Page.navigate({url: testRunner.url('/resources/blank.html')});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 1000});

  await dp.Emulation.onceVirtualTimeBudgetExpired();
  await dp.Performance.getMetrics();
  // Should pass.
  testRunner.log('Returned from the Performance.getMetrics call.');
  testRunner.completeTest();
})
