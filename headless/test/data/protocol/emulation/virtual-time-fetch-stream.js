// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that fetching a stream with back pressure ' +
      'does not stall with virtual time enabled.');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`<html></html>`)
  );

  helper.onceRequest('http://test.com/fetch').fulfill(
      FetchHelper.makeResponse('<some data>',
          ["Cache-Control: no-store"])
  );

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    testRunner.log(await session.evaluate('document.title'));
    testRunner.completeTest();
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  await session.evaluateAsync(`fetch('/fetch')`);
})
