// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests virtual time basics.`);
  dp.Emulation.onVirtualTimeBudgetExpired(data => testRunner.completeTest());

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: testRunner.url('/resources/blank.html')});
  await dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 1000});
})
