// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that VT advances in lockstep between worker and host page.`);

  const WorkerVirtualTimeHelper = await testRunner.loadScript(
      '../helpers/worker-virtual-time-helper.js');
  const workerVirtualTimeHelper =
      new WorkerVirtualTimeHelper(testRunner, session);
  const { wp } = await workerVirtualTimeHelper.loadWorker(`
        setInterval(() => {
          self.postMessage('Time in worker: ' + Date.now());
        }, 100);
  `);
  dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(({params}) => {
    testRunner.log(params.args[0].value);
  });
  await session.evaluate(`(async function() {
    await new Promise(resolve => setTimeout(resolve, 50));
    worker.addEventListener('message',
        event => {console.log('[WORKER] ' + event.data)}, false);
    setInterval(() => { console.log('Time in page: ' + Date.now()); }, 100);
  })()`);
  wp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 1000});
  dp.Emulation.onVirtualTimeBudgetExpired(() => {
    dp.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending',
    budget: 1000});
  });
  wp.Runtime.runIfWaitingForDebugger();
  await wp.Emulation.onceVirtualTimeBudgetExpired();
  wp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 1000});
    await wp.Emulation.onceVirtualTimeBudgetExpired();
    testRunner.completeTest();
})