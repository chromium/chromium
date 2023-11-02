// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that virtual time advances 10ms on every navigation.`);
  await dp.Network.enable();

  let resourceCounter = 0;
  dp.Network.onRequestWillBeSent(() => { resourceCounter++ });
  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({
      url: testRunner.url('resources/virtual-time-error-loop.html')});
  dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending',
      budget: 5000,
      maxVirtualTimeTaskStarvationCount: 1000000});  // starvation prevents flakes
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log('Resources loaded: ' + resourceCounter);
  testRunner.completeTest();
})
