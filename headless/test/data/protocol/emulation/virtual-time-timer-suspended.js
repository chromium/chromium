// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that dom timer respects virtual time.`);
  await dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await session.evaluate(`
      setTimeout(() => { console.log(1000); }, 1000);
      setTimeout(() => { console.log(1001); }, 1001);`);

  testRunner.log('Grant first budget');
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 1001});

  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log('Grant second budget');
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 1});
  await dp.Emulation.onceVirtualTimeBudgetExpired();

  testRunner.completeTest();
})
