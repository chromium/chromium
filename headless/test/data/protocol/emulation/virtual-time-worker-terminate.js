// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests virtual time' +
      ' is unpaused when a worker is terminated before start.');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const { result: { sessionId } } =
      await testRunner.browserP().Target.attachToBrowserTarget({});
  const { protocol: bp } = new TestRunner.Session(testRunner, sessionId);
  const fetcher = new FetchHelper(testRunner, bp);

  await fetcher.enable();

  fetcher.onceRequest('http://test.com/index.html').fulfill(
    FetchHelper.makeContentResponse(`<html><script>
          window.onload = function() {
            window.worker = new Worker('/worker.js');
            window.worker.terminate();
            setTimeout(() => console.log('timer fired'), 100);
          };
        </script>
        </html>
    `));
  fetcher.onceRequest('http://test.com/worker.js').fulfill(
    FetchHelper.makeContentResponse(`console.log('worker')`));

  dp.Runtime.onConsoleAPICalled(event =>
     testRunner.log(event.params.args[0].value));
  await dp.Runtime.enable();
  await dp.Emulation.setVirtualTimePolicy({
    policy: 'pause',
    initialVirtualTime: 100
  });

  await dp.Page.navigate({url: 'http://test.com/index.html'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending',
      budget: 1000});

  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log('PASSED: virtual time budget expired');
  testRunner.completeTest();
})
