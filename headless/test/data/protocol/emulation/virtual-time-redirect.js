// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that redirects don't break virtual time.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <iframe src="/iframe.html" width="400" height="200"
          id="iframe1"></iframe>`)
  );

  helper.onceRequest('http://test.com/iframe.html').fulfill(
      FetchHelper.makeRedirectResponse('iframe2.html')
  );

  helper.onceRequest('http://test.com/iframe2.html').fulfill(
      FetchHelper.makeContentResponse(`
          <link rel="stylesheet" type="text/css" href="style.css">
          <h1>Hello from the iframe!</h1>`)
  );

  helper.onceRequest('http://test.com/style.css').fulfill(
      FetchHelper.makeRedirectResponse('style2.css')
  );

  helper.onceRequest('http://test.com/style2.css').fulfill(
      FetchHelper.makeContentResponse(`.test { color: blue; }`, "text/css")
  );

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.completeTest();
})
