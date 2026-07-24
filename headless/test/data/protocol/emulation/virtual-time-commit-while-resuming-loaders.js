// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that navigations committing while paused document loaders are ' +
      'being resumed are handled correctly.');

  const FetchHelper =
      await testRunner.loadScriptAbsolute('../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  helper.setEnableLogging(false);
  await helper.enable();

  const kFrameCount = 8;

  helper.onceRequest('http://test.com/index.html')
      .fulfill(FetchHelper.makeContentResponse(`<!doctype html><body></body>`));
  await session.navigate('http://test.com/index.html');
  await session.evaluate(`
      for (let i = 0; i <= ${kFrameCount}; ++i) {
        const f = document.createElement('iframe');
        f.id = 'f' + i;
        document.body.appendChild(f);
      }`);

  await dp.Page.enable();
  await dp.Debugger.enable();
  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});

  // Navigate child frames so their document loaders commit while virtual
  // time is paused and are deferred until a budget is granted.
  const childContent =
      `<!doctype html><script>debugger;document.title='done';</script>`;
  for (let i = 0; i < kFrameCount; ++i) {
    helper.onceRequest(`http://test.com/child${i}.html`)
        .fulfill(FetchHelper.makeContentResponse(childContent));
  }
  let committed = 0;
  dp.Page.onFrameNavigated(() => ++committed);
  for (let i = 0; i < kFrameCount; ++i) {
    session.evaluate(
        `document.getElementById('f${i}').src = '/child${i}.html'`);
  }
  while (committed < kFrameCount)
    await dp.Page.onceFrameNavigated();

  // Start one more navigation but hold the response so it commits later.
  const lateRequest = helper.onceRequest('http://test.com/late.html').matched();
  session.evaluate(
      `document.getElementById('f${kFrameCount}').src = '/late.html'`);
  const lateRequestId = (await lateRequest).requestId;

  // Resuming the deferred loaders parses the first child synchronously and
  // pauses in the debugger. While paused, the budget expires and the held
  // navigation is allowed to commit, which adds another deferred loader.
  let lateCommitted;
  dp.Debugger.onPaused(async () => {
    if (!lateCommitted) {
      await dp.Emulation.onceVirtualTimeBudgetExpired();
      lateCommitted = dp.Page.onceFrameNavigated();
      dp.Fetch.fulfillRequest(Object.assign(
          FetchHelper.makeContentResponse(
              `<!doctype html><script>document.title='done';</script>`),
          {requestId: lateRequestId}));
      // Resume debugger before waiting for navigation commit to prevent
      // C++ DCHECK(!ParentDocument()->domWindow()->IsContextPaused()) failure
      // in Document::Document(). Since the 1ms virtual time budget has already
      // expired, virtual time remains paused when late.html commits.
      dp.Debugger.resume();
      await lateCommitted;
    } else {
      dp.Debugger.resume();
    }
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'advance', budget: 1});
  await lateCommitted;

  // Resume the loader that committed while the others were being resumed.
  dp.Emulation.setVirtualTimePolicy({policy: 'advance', budget: 1000});
  await dp.Emulation.onceVirtualTimeBudgetExpired();

  testRunner.log(
      'Loaded child frames: ' +
      await session.evaluate(`Array.from(document.querySelectorAll('iframe'),
                  f => f.contentDocument.title).filter(t => t).length`));

  testRunner.completeTest();
})
