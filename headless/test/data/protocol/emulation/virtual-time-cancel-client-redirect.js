// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const html = `
    <script>
      setTimeout(() => console.log('DONE'), 1000);
      location.href = '/redirect';
    </script>`;

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startWithFrameControl(
      `Tests that virtual time isn't stalled by aborted client-side redirect.`);

  await dp.Runtime.enable();
  await dp.Fetch.enable();

  dp.Fetch.onRequestPaused(event => {
    const url = event.params.request.url;
    if (/index.html$/.test(url)) {
      dp.Fetch.fulfillRequest({
        requestId: event.params.requestId,
        responseCode: 200,
        responseHeaders: [],
        body: btoa(html)
      });
      return;
    }
    testRunner.log(`Aborting request to ${url}`);
    dp.Fetch.failRequest({
      requestId: event.params.requestId,
      errorReason: 'Aborted'
    });
  });

  const virtualTimeChunkSize = 100;
  const virtualTimeTicksBase =
     (await dp.Emulation.setVirtualTimePolicy({policy: 'pause'}))
         .result.virtualTimeTicksBase;
  dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending',
      budget: virtualTimeChunkSize,
      waitForNavigation: true});

  let frameTimeTicks = 0;
  dp.Emulation.onVirtualTimeBudgetExpired(async e => {
    frameTimeTicks += virtualTimeChunkSize;
    const frameArgs = {
      frameTimeTicks: virtualTimeTicksBase + frameTimeTicks,
      interval: virtualTimeChunkSize,
      noDisplayUpdates: false
    };
    if (frameTimeTicks > 500)
      frameArgs.screenshot = {format: 'png', quality: 100};
    await dp.HeadlessExperimental.beginFrame(frameArgs);
    await dp.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending', budget: virtualTimeChunkSize});
  });

  dp.Page.navigate({url: 'http://test.com/index.html'});

  const params = (await dp.Runtime.onceConsoleAPICalled()).params;
  testRunner.log(`page says: ${params.args[0].value}`);
  testRunner.completeTest();
})
