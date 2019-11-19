// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that detaching frame while issuing request doesn't break virtual time.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  dp.Emulation.onVirtualTimeBudgetExpired(data => testRunner.completeTest());

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  dp.Page.navigate({url: 'http://test.com/index.html'});

  await helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <iframe src="detach-frame-iframe.html"
          width="400" height="200" id="iframe1"></iframe>`)
  );

  await helper.onceRequest('http://test.com/detach-frame-iframe.html').fulfill(
      FetchHelper.makeContentResponse(`
          <link rel="stylesheet" type="text/css" href="detach-frame-style.css">
          <h1>Hello from the iframe!</h1>`)
  );

  // FetchHelper does not provide for imperative code, so avoid using it when
  // we need to detach iframe with the request in-flight.
  const params = (await dp.Fetch.onceRequestPaused()).params;
  await session.evaluate(`document.getElementById('iframe1').remove()`);
  await dp.Fetch.fulfillRequest({
    requestId: params.requesId,
    responseCode: 200,
    responseHeaders: [{name: 'Content-type', value: 'text/css'}],
    body: btoa(`.test { color: blue; }`)
  });

})
