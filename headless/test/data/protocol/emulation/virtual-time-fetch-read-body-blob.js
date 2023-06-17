// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests that Virtual Time' +
      ' does not deadlock with Response.blob() and URL.createObjectURL()');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
    FetchHelper.makeContentResponse(`<html></html>`)
  );
  for (let i = 0; i < 10; ++i) {
    helper.onceRequest(`http://test.com/fetch/blob${i}`).matched().then(params => {
      setTimeout(() => {
        const response = FetchHelper.makeContentResponse(`blob-${i}`);
        response.requestId = params.requestId;
        dp.Fetch.fulfillRequest(response);
      }, 10);
    });
  }

  dp.Runtime.enable();
  // Defer logging of console messages so these do not intervine with
  // interception-side logging.
  const consoleMessages = [];
  dp.Runtime.onConsoleAPICalled(({params}) => {
    consoleMessages.push(params.args[0].value);
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
  session.evaluate(`(async function() {
    const fetches = [];
    for (let i = 0; i < 10; ++i)
      fetches.push(fetch('/fetch/blob' + i));

    const p = await Promise.all(fetches.map(f =>
      f.then(r => r.blob())
        .then(b => fetch(URL.createObjectURL(b)))
        .then(f => f.text())));
    console.log('got blobs ' + p.join(','));
  })()`);
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});

  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log(consoleMessages);
  testRunner.completeTest();
})
