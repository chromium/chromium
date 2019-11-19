// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that virtual time does not hang when a fetch is cancelled ' +
      'due to exceeded size of pending keepalive data.');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('https://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`<html></html>`));

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  await dp.Page.navigate({url: 'https://test.com/index.html'});

  helper.onceRequest('https://test.com/post').matched().then(() =>
      testRunner.log('FAIL: this request should not come through!'));

  await session.evaluateAsync(`
      fetch('/post', {method: 'POST', body: '*'.repeat(64 * 1024 + 1),
          keepalive: true})
  `);
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.completeTest();
})
