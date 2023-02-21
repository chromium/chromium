// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests virtual time is paused while a fetched is pending in workers.`);

  const WorkerVirtualTimeHelper = await testRunner.loadScript(
    '../helpers/worker-virtual-time-helper.js');

  const workerVirtualTimeHelper =
      new WorkerVirtualTimeHelper(testRunner, session);
  const { wp, fetcher, FetchHelper } =
      await workerVirtualTimeHelper.loadWorker(`
          (async function() {
            console.log('Started worker: ' + Date.now());
            // Make sure fetch starts after worker script is loaded.
            await new Promise(resolve => setTimeout(resolve, 5));
            const response = await fetch('test.txt');
            console.log('fetch response: ' + await response.text());
            console.log('Time after fetch: ' + Date.now());
          })();
  `);
  fetcher.onceRequest('http://test.com/test.txt').fulfill(
    FetchHelper.makeContentResponse('hello, world!'));

  wp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 100});
  wp.Runtime.enable();
  wp.Runtime.onConsoleAPICalled(({params}) => {
    testRunner.log(params.args[0].value);
  });
  wp.Runtime.runIfWaitingForDebugger();

  await wp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log('Virtual time budget expired');
  testRunner.completeTest();
})
