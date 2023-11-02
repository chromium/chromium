// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that dom timer respect virtual time order.`);

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await session.evaluate(`
      var run_order = [];
      function timerFn(delay, value) {
        setTimeout(() => { run_order.push(value); }, delay);
      };
      var one_minute = 60 * 1000;
      timerFn(one_minute * 4, 'a');
      timerFn(one_minute * 2, 'b');
      timerFn(one_minute, 'c');`);

  // Normally the JS runs pretty much instantly but the timer callbacks will
  // take 4 mins to fire, but thanks to timer fast forwarding we can make them
  // fire immediatly.
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 60 * 1000 * 4});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log(await session.evaluate(`run_order.join(', ')`));
  testRunner.completeTest();
})
