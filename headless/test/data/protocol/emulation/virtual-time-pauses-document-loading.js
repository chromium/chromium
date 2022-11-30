// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that document loading is paused while virtual time is paused');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  const content = `
    <html>
    <script>
      console.log("time at inline script: " + (new Date()).getTime());
    </script>
    </html>
  `;
  helper.onceRequest('https://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(content));

  await dp.Emulation.setVirtualTimePolicy({
    policy: 'pause',
    initialVirtualTime: 100000
  });
  await dp.Page.navigate({url: 'https://test.com/index.html'});

  // The loader should be paused at this time, so no console.log()
  // from the inline script is yet executed. Let's see if we can
  // sneak in before it gets run.
  await session.evaluate(`
    console.log("time at eval while paused: " + (new Date()).getTime());
  `);

  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 5000,
  });

  dp.Runtime.enable();
  for (let i = 0; i < 2; ++i) {
    const message = (await dp.Runtime.onceConsoleAPICalled()).params;
    testRunner.log(`[page] ${message.args[0].value}`);
  }
  testRunner.completeTest();
});
