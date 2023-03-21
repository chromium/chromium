// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests virtual time with history navigation.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const fetcher = new FetchHelper(testRunner, dp);
  await fetcher.enable();

  fetcher.onceRequest('http://test.com/index.html').fulfill(
    FetchHelper.makeContentResponse(`<html><img src="circle.svg"><html>`));

  fetcher.onceRequest('http://test.com/circle.svg').fulfill(
    FetchHelper.makeContentResponse(
        `<?xml version="1.0" encoding="iso-8859-1"?>
        <svg width="100" height="100"
            xmlns="http://www.w3.org/2000/svg"
            xmlns:xlink="http://www.w3.org/1999/xlink">
            <circle cx="50" cy="50" r="50" fill="green" />
        </svg>
    `, 'image/svg+xml'));

  await dp.Page.enable();
  dp.Page.navigate({url: 'http://test.com/index.html'});
  await dp.Page.onceLoadEventFired();
  dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.completeTest();
})
