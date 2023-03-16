// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests basic virtual time functionality in workers.`);

  const WorkerVirtualTimeHelper = await testRunner.loadScript(
      '../helpers/worker-virtual-time-helper.js');
  const workerVirtualTimeHelper =
      new WorkerVirtualTimeHelper(testRunner, session);
  const { wp } = await workerVirtualTimeHelper.loadWorker(`
      (async function() {
        console.log('Started worker: ' + Date.now());
        await new Promise(resolve => setTimeout(resolve, 500));
        console.log('After a 500ms timeout: ' + Date.now());
      })();
  `);

  wp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 5000});
  wp.Runtime.enable();
  wp.Runtime.onConsoleAPICalled(({params}) => {
    testRunner.log(params.args[0].value);
  });
  wp.Runtime.runIfWaitingForDebugger();
  // From now on, VT runs in both page and the worker and one will
  // block another if VT expires, so make sure we drive the time in
  // page to avoid worker being blocked on the page.
  dp.Emulation.onVirtualTimeBudgetExpired(() => {
    dp.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending',
    budget: 5000});
  });

  await wp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log(`virtual time expired, granting more`);
  wp.Runtime.evaluate({
    expression: `
        (async function() {
          console.log('After first budget expired: ' + Date.now());
          await new Promise(resolve => setTimeout(resolve, 500));
          console.log('Another 500ms later: ' + Date.now());
        })()`
  });

  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  wp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  await wp.Emulation.onceVirtualTimeBudgetExpired();

  testRunner.completeTest();
})